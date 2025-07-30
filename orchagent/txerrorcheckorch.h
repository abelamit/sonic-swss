#pragma once

#include <array>
#include <linux/timer.h>
#include "orch.h"
#include "dbconnector.h"
#include "table.h"

extern "C" {
    #include "sai.h"
}

#define TX_ERROR_CHECK_THRESHOLD_DEFAULT   1
#define TX_ERROR_CHECK_POLL_TIMEOUT_SEC_DEFAULT   5 

class TxErrorCheckOrch: public Orch
{
public:
    TxErrorCheckOrch(swss::DBConnector *db, const std::string &tableName);
    virtual ~TxErrorCheckOrch(void);
    void doTask(swss::SelectableTimer &timer);
    void doTask(Consumer &consumer);

private:
    void mcCounterCheck();
    void mcFieldsUpdate(swss::KeyOpFieldsValuesTuple keyOpFieldsValues);
    void mcUpdateThreshold(uint64_t new_threshold);
    void mcUpdateTimePeriod(time_t new_time_period);
    void setPortStatus(std::string port_name, bool isTxErrorState);

    std::shared_ptr<swss::DBConnector> m_countersDb = nullptr;
    std::unique_ptr<swss::Table> m_countersTable = nullptr;
    std::unique_ptr<swss::DBConnector> m_configDb = nullptr;
    std::unique_ptr<swss::DBConnector> m_stateDb = nullptr;

    uint64_t m_error_threshold = TX_ERROR_CHECK_THRESHOLD_DEFAULT;  
    std::shared_ptr<swss::SelectableTimer> m_timer = nullptr;    
};
