#include <cstring>

#define MAX_STRING_LENGTH 64

class AppState {
   private:
    static RTC_DATA_ATTR char uiState[MAX_STRING_LENGTH];
    static RTC_DATA_ATTR uint32_t checksum;
    esp_sleep_wakeup_cause_t wakeup_reason;

   public:
    AppState() {
        this->wakeup_reason = esp_sleep_get_wakeup_cause();
    }

    uint32_t getChecksum() const {
        return this->checksum;
    }
    void setChecksum(uint32_t val) {
        this->checksum = val;
    }

    const char* getUiState() const {
        return this->uiState;
    }
    void setUiState(const std::string &state) {
        strncpy(this->uiState, state.c_str(), MAX_STRING_LENGTH - 1);
        this->uiState[MAX_STRING_LENGTH - 1] = '\0';
    }

    esp_sleep_wakeup_cause_t getWakeupReason() {
        return this->wakeup_reason;
    }
    bool isWakeupReason(esp_sleep_wakeup_cause_t reason) {
        return this->getWakeupReason() == reason;
    }
};

RTC_DATA_ATTR uint32_t AppState::checksum = 0;
RTC_DATA_ATTR char AppState::uiState[MAX_STRING_LENGTH] = "";