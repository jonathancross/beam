package main

import (
	"fmt"
	"github.com/olahol/melody"
	"log"
)

var walletServices *Services
var sbbsServices *Services

func monitorInitialize(m *melody.Melody) (err error) {
	walletServices, err = NewWalletServices()
	if err != nil {
		return
	}

	sbbsServices, err = NewBbsServices()
	if err != nil {
		return
	}

	//
	// This is a connection point between services and endpoints
	//
	go func () {
		for {
			select {
			case svcIdx := <- walletServices.Dropped:
				points, clients := epoints.DropServiceEndpoints(svcIdx)
				log.Printf("service %v, dropped. %v endpoint(s) with %v client(s)", svcIdx, points, clients)
			case svcIdx := <- walletServices.Restarted:
				log.Printf("service %v, restarted", svcIdx)
			}
		}
	} ()

	//
	// This is connection point between sbbs service and sbbs watchers
	//
	go func () {
		for {
			select {
			case svcIdx := <- sbbsServices.Dropped:
				log.Printf("bbs %v, dropped", svcIdx)
			case svcIdx := <- sbbsServices.Restarted:
				log.Printf("bbs %v, restarted", svcIdx)
			}
		}
	} ()

	// Start listening for SBBs messages
	sbbsListen()

	return
}

func monitorGet(wid string) (string, error) {
	if epoint, ok := epoints.Get(wid); ok {
		svcIdx, svcAddr := epoint.Use()
		log.Printf("wallet %v, existing endpoint is [%v:%v]", wid, svcIdx, svcAddr)
		return svcAddr, nil
	}

	//
	// Since balancer is concurrent new endpoint might be added by another
	// thread between epoints.Get and epoints.Add. Add() handles this case
	// and returns existing endpoint is necessary. This situation should
	// be very rare though possible
	//
	svcIdx, service, err := walletServices.GetNext()
	if err != nil {
		return "", fmt.Errorf("wallet %v, %v", wid, err)
	}

	epoints.Add(wid, svcIdx, service.Address)
	log.Printf("wallet %v, new endpoint is [%v:%v]", wid, svcIdx, service.Address)
	return service.Address, nil
}

func monitorAlive (wid string) error {
	if epoint, ok := epoints.Get(wid); ok {
		epoint.WalletAlive <- true
		return nil
	}

	// This usually means issue in the web wallet code
	return fmt.Errorf("wallet %v, alive request on missing endpoint", wid)
}

func monitorLogout(wid string) error {
	if epoint, ok := epoints.Get(wid); ok {
		epoint.WalletLogout <- true
		return nil
	}

	// This usually means issue in the web wallet code
	return fmt.Errorf("wallet %v, logout request on missing endpoint", wid)
}
