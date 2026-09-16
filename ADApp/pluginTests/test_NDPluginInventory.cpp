/*
 * test_NDPluginInventory.cpp
 *
 *  Created on: 15 Sep 2026
 *      Author: Jakub Wlodek
 */
#include <string>
#include <vector>

#include "boost/test/unit_test.hpp"

#include <NDPluginCircularBuff.h>
#include <NDPluginInventory.h>
#include <asynNDArrayDriver.h>
#include <asynDriver.h>
#include <asynOctetSyncIO.h>

#include "testingutilities.h"

using namespace std;

struct PluginInventoryTestFixture
{
    asynNDArrayDriver *dummy_driver;
    NDPluginCircularBuff *cb1;
    NDPluginCircularBuff *cb2;
    string port1, port2;

    /** Create a dummy port driver with two plugins. Make sure to start them. */
    PluginInventoryTestFixture
    {
        string dummy_port("simInvPort");
        port1 = "invCB1";
        port2 = "invCB2";
        uniqueAsynPortName(dummy_port);
        uniqueAsynPortName(port1);
        uniqueAsynPortName(port2);

        dummy_driver = new asynNDArrayDriver(dummy_port.c_str(), 1, 0, 0,
                asynGenericPointerMask, asynGenericPointerMask, 0, 0, 0, 0);

        // Construction will initially register the plugins with just port names
        cb1 = new NDPluginCircularBuff(port1.c_str(), 50, 0, dummy_port.c_str(), 0, 1000, -1, 0, 2000000);
        cb2 = new NDPluginCircularBuff(port2.c_str(), 50, 0, dummy_port.c_str(), 0, 1000, -1, 0, 2000000);

        // Plugin types are set in start()
        cb1->start();
        cb2->start();

        // PV prefixes are set in writeOctet() when the PvPrefix record PINI is processed.
        // Simulate that record processing with a put to the PLUGIN_PV_PREFIX parameter.
        simulatePvPrefixPut(port1, "TEST:CB1:");
        simulatePvPrefixPut(port2, "TEST:CB2:");
    }

    // Simulate a caput to a plugin's PvPrefix record by writing through its asyn octet interface.
    void simulatePvPrefixPut(const string &portName, const string &pvPrefix)
    {
        asynUser *pasynUser = NULL;
        asynStatus status = pasynOctetSyncIO->connect(portName.c_str(), 0, &pasynUser,
                NDPluginDriverPvPrefixString);
        BOOST_REQUIRE_EQUAL(status, asynSuccess);
        size_t nwrite = 0;
        status = pasynOctetSyncIO->write(pasynUser, pvPrefix.c_str(), pvPrefix.size(),
                1.0, &nwrite);
        BOOST_CHECK_EQUAL(status, asynSuccess);
        pasynOctetSyncIO->disconnect(pasynUser);
    }
    ~PluginInventoryTestFixture
    {
        delete cb2;
        delete cb1;
        delete dummy_driver;
    }

    // Finds the plugin for a given port within a type's plugin list.
    const NDPluginInventory::RegisteredPlugin *findPlugin(
            const vector<NDPluginInventory::RegisteredPlugin> &plugins,
            const string &portName)
    {
        for (size_t i = 0; i < plugins.size(); i++) {
            if (plugins[i].portName == portName) return &plugins[i];
        }
        return NULL;
    }
};

BOOST_FIXTURE_TEST_SUITE(PluginInventoryTests, PluginInventoryTestFixture

// Plugins group under their plugin type, carrying port name and PV prefix.
BOOST_AUTO_TEST_CASE(test_InventoryKeyedByType)
{
    NDPluginInventory::Inventory inv = NDPluginInventory::getInventory();

    NDPluginInventory::Inventory::const_iterator it = inv.find("NDPluginCircularBuff");
    BOOST_REQUIRE(it != inv.end());

    const vector<NDPluginInventory::RegisteredPlugin> &plugins = it->second;
    const NDPluginInventory::RegisteredPlugin *e1 = findPlugin(plugins, port1);
    const NDPluginInventory::RegisteredPlugin *e2 = findPlugin(plugins, port2);

    BOOST_REQUIRE(e1 != NULL);
    BOOST_REQUIRE(e2 != NULL);
    BOOST_CHECK_EQUAL(e1->pvPrefix, "TEST:CB1:");
    BOOST_CHECK_EQUAL(e2->pvPrefix, "TEST:CB2:");
}

// A plugin is removed from the inventory when it is destroyed.
BOOST_AUTO_TEST_CASE(test_UnregisterOnDelete)
{
    delete cb2;
    cb2 = NULL;

    NDPluginInventory::Inventory inv = NDPluginInventory::getInventory();
    NDPluginInventory::Inventory::const_iterator it = inv.find("NDPluginCircularBuff");
    BOOST_REQUIRE(it != inv.end());

    BOOST_CHECK(findPlugin(it->second, port2) == NULL);
    BOOST_CHECK(findPlugin(it->second, port1) != NULL);
}

BOOST_AUTO_TEST_SUITE_END()
