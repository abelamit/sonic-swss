import time
import pytest

from swsscommon import swsscommon

TX_ERROR_CHECK_POLL_TIMEOUT_SEC_DEFAULT   = (5 * 60)
TX_ERROR_CHECK_THRESHOLD_DEFAULT   = (5 * 60)

CFG_TX_ERROR_CHECK_TABLE_NAME = "CFG_TX_ERROR_CHECK"
TX_ERROR_CHECK_KEY = "TX_ERROR_CHECK"
TX_ERROR_CHECK_POLL_NAME = "TX_ERROR_CHECK_POLL"
THRESHOLD_FIELD = "threshold"
TIME_PERIOD_FIELD = "time_period"

TX_ERROR_PORT_STATE_FIELD = "tx_error_port_state"
TX_ERROR_PORT_STATE_ERROR = "error"
TX_ERROR_PORT_STATE_OK = "ok"

# port to be tested
PORT = "Ethernet0"

@pytest.mark.usefixtures('dvs_port_manager')
class TestTxErrorCounters(object):
    def setup_db(self, dvs):
        self.asic_db = swsscommon.DBConnector(1, dvs.redis_sock, 0)
        self.config_db = swsscommon.DBConnector(4, dvs.redis_sock, 0)
        self.flex_db = swsscommon.DBConnector(5, dvs.redis_sock, 0)
        self.state_db = swsscommon.DBConnector(6, dvs.redis_sock, 0)
        self.counters_db = swsscommon.DBConnector(2, dvs.redis_sock, 0)

    def genericGetAndAssert(self, table, key):
        status, fields = table.get(key)
        assert status
        return fields

    def set_tx_error_config(self, field, value):
        tx_error_table = swsscommon.Table(self.config_db, CFG_TX_ERROR_CHECK_TABLE_NAME)
        entry = swsscommon.FieldValuePairs([(field, value)])
        tx_error_table.set(TX_ERROR_CHECK_KEY, entry)