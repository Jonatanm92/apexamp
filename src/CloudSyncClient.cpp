#include "CloudSyncClient.h"

namespace namapp {
namespace cloud {

CloudSyncClient::CloudSyncClient() {}
CloudSyncClient::~CloudSyncClient() {}

void CloudSyncClient::login(const std::string& username, const std::string& password, std::function<void(bool)> onResult) {
    // Mock network request
    mAuthToken = "mock_jwt_token_123";
    if (onResult) onResult(true);
}

void CloudSyncClient::searchCommunityPresets(const std::string& query, std::function<void(std::vector<Preset>)> onResult) {
    // Mock JSON response parsing
    std::vector<Preset> results = {
        {"p1", "Thall God 3000", "Buster", "Thall", "https://api.namapp.cloud/dl/p1"},
        {"p2", "Djenty Clean", "Vild", "Clean", "https://api.namapp.cloud/dl/p2"}
    };
    
    if (onResult) onResult(results);
}

void CloudSyncClient::downloadPreset(const std::string& presetId, const std::string& targetPath, std::function<void(bool)> onResult) {
    // Mock downloading and saving .nam / .wav files
    if (onResult) onResult(true);
}

} // namespace cloud
} // namespace namapp
