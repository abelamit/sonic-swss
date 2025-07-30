#include "txerrorcheckorch.h"
#include "select.h"
#include "notifier.h"
#include "sai_serialize.h"
#include "portsorch.h"
#include <inttypes.h>

extern sai_port_api_t *sai_port_api;
extern PortsOrch *gPortsOrch;

#define TX_ERROR_CHECK_KEY "TX_ERROR_CHECK"
#define TX_ERROR_CHECK_POLL_NAME "TX_ERROR_CHECK_POLL"  
#define THRESHOLD_FIELD "threshold"
#define TIME_PERIOD_FIELD "time_period"

#define TX_ERROR_PORT_STATE_FIELD "tx_error_port_state"
#define TX_ERROR_PORT_STATE_ERROR "error"
#define TX_ERROR_PORT_STATE_OK "ok"

TxErrorCheckOrch::TxErrorCheckOrch(swss::DBConnector *db, const std::string &tableName):
    Orch(db, tableName)
{
    SWSS_LOG_ENTER();

    m_countersDb = make_shared<swss::DBConnector>("COUNTERS_DB", 0);
    m_countersTable = make_unique<swss::Table>(m_countersDb.get(), COUNTERS_TABLE);
    m_configDb = make_unique<swss::DBConnector>("CONFIG_DB", 0);
    m_stateDb = make_unique<swss::DBConnector>("STATE_DB", 0);

    auto interv = timespec { .tv_sec = TX_ERROR_CHECK_POLL_TIMEOUT_SEC_DEFAULT, .tv_nsec = 0 };
    m_timer = std::make_shared<swss::SelectableTimer>(interv);
    auto executor = new ExecutableTimer(m_timer.get(), this, TX_ERROR_CHECK_POLL_NAME);
    Orch::addExecutor(executor);
    m_timer->start();
}

TxErrorCheckOrch::~TxErrorCheckOrch(void)
{
    SWSS_LOG_ENTER();
}

void TxErrorCheckOrch::doTask(swss::SelectableTimer &timer)
{
    SWSS_LOG_ENTER();

    mcCounterCheck();
}

void TxErrorCheckOrch::doTask(Consumer &consumer)
{
    SWSS_LOG_ENTER();
    
    if (consumer.getTableName() != CFG_TX_ERROR_CHECK_TABLE_NAME)
    {
        SWSS_LOG_ERROR("Unknown table name %s", consumer.getTableName().c_str());
        return;
    }

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        mcFieldsUpdate(it->second);
        it = consumer.m_toSync.erase(it);
    }
}

void TxErrorCheckOrch::mcCounterCheck()
{
    SWSS_LOG_ENTER();

    for (auto const &port : gPortsOrch->getAllPorts())
    {
        sai_object_id_t  portOid = port.second.m_port_id;
        if (portOid == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_ERROR("Invalid port oid %lx" PRIx64, port.second.m_port_id);
            continue;
        }

        std::string outErrors;

        if (!m_countersTable->hget(sai_serialize_object_id(portOid), "SAI_PORT_STAT_IF_OUT_ERRORS", outErrors)) 
        {
            SWSS_LOG_ERROR("Access to Counters DB with %lx port ID failed", port.second.m_port_id);
            continue;
        }

        uint32_t outErrorsCount = to_uint<uint32_t>(outErrors);
        
        /* Note: for now the support is only for error state true; if needed we can add support for error state false. */
        if (outErrorsCount > m_error_threshold)
        {
            setPortStatus(port.second.m_alias, true);
        }        
    }  
}

void TxErrorCheckOrch::mcFieldsUpdate(swss::KeyOpFieldsValuesTuple keyOpFieldsValues)
{
    SWSS_LOG_ENTER();

    string key = kfvKey(keyOpFieldsValues);

    if (key != TX_ERROR_CHECK_KEY)
    {
        SWSS_LOG_ERROR("Unknown key %s", key.c_str());
        return;
    }

    auto op = kfvOp(keyOpFieldsValues);
    if ((op != DEL_COMMAND) && (op != SET_COMMAND))
    {
        SWSS_LOG_ERROR("Unknown operation %s", op.c_str());
        return;
    }

    for (auto fvMap : kfvFieldsValues(keyOpFieldsValues))
    {
        auto fieldName = fvField(fvMap);
        auto fieldValue = fvValue(fvMap);

        if (fieldName == THRESHOLD_FIELD)
        {
            if (op == DEL_COMMAND)
            {
                fieldValue = std::to_string(TX_ERROR_CHECK_THRESHOLD_DEFAULT);
            }

            mcUpdateThreshold(to_uint<uint64_t>(fieldValue));
        }
        else if (fieldName == TIME_PERIOD_FIELD)
        {
            if (op == DEL_COMMAND)
            {
                fieldValue = std::to_string(TX_ERROR_CHECK_POLL_TIMEOUT_SEC_DEFAULT);
            }

            mcUpdateTimePeriod(to_uint<time_t>(fieldValue));
        }
    }
}

void TxErrorCheckOrch::mcUpdateThreshold(uint64_t new_threshold)
{
    SWSS_LOG_ENTER();

    m_error_threshold = new_threshold;
}

void TxErrorCheckOrch::mcUpdateTimePeriod(time_t new_time_period)
{
    SWSS_LOG_ENTER();

    auto new_interv = timespec { .tv_sec = new_time_period, .tv_nsec = 0 };
    m_timer->setInterval(new_interv);
    m_timer->reset();
}

void TxErrorCheckOrch::setPortStatus(std::string port_name, bool isTxErrorState) 
{
    SWSS_LOG_ENTER();

    swss::Table portStateTable(m_stateDb.get(), STATE_PORT_TABLE_NAME);
    portStateTable.hset(port_name, TX_ERROR_PORT_STATE_FIELD, (isTxErrorState ? TX_ERROR_PORT_STATE_ERROR : TX_ERROR_PORT_STATE_OK));
}
