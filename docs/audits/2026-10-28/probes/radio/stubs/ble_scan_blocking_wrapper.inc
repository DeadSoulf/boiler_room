BLEScanResults *BLEScan::start(uint32_t duration, bool is_continue) {
  if (duration == 0) {
    log_w("Blocking scan called with duration = forever");
  }

  if (m_pTaskData != nullptr) {
    log_e("Scan already in progress");
    return &m_scanResults;
  }

  BLETaskData taskData(this);
  m_pTaskData = &taskData;

  if (start(duration, nullptr, is_continue)) {
    BLEUtils::taskWait(taskData, BLE_NPL_TIME_FOREVER);
  }

  m_pTaskData = nullptr;
  return &m_scanResults;
}  // start
