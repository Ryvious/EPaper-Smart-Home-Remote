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

    void print_wakeup_reason() {
        static const char *TAG = "ESP";

        switch (this->getWakeupReason()) {
            case ESP_SLEEP_WAKEUP_EXT0:
                ESP_LOGI(TAG, "Wakeup caused by external signal using RTC_IO");
                break;

            case ESP_SLEEP_WAKEUP_EXT1:
                ESP_LOGI(TAG, "Wakeup caused by external signal using RTC_CNTL");
                break;

            case ESP_SLEEP_WAKEUP_TIMER:
                ESP_LOGI(TAG, "Wakeup caused by timer");
                break;

            case ESP_SLEEP_WAKEUP_TOUCHPAD:
                ESP_LOGI(TAG, "Wakeup caused by touchpad");
                break;

            case ESP_SLEEP_WAKEUP_ULP:
                ESP_LOGI(TAG, "Wakeup caused by ULP program");
                break;

            default:
                ESP_LOGI(TAG, "Wakeup was not caused by deep sleep: %d", wakeup_reason);
                break;
        }
    }
};

RTC_DATA_ATTR uint32_t AppState::checksum = 0;
RTC_DATA_ATTR char AppState::uiState[MAX_STRING_LENGTH] = "";