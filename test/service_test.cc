#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <sys/time.h>
#include <thread>
#include <vector>

#include <glog/logging.h>
#include "gtest/gtest.h"

#include "service_client_lib.h"
#include "service_data_structure.h"

namespace {

uint64_t PrintId(const std::string &id) {
  return *(reinterpret_cast<const uint64_t*>(id.c_str()));
}

const size_t kNumOfUsersPreset = 10;
const size_t kNumOfUsersTotal = 20;
const char *kShortText = "short";
const char *kLongText = "longlonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglonglong";

// This test cases on the `ServiceDataStructure` to check whether their
// interfaces work correctly.
class ServiceTestDataStructure : public ::testing::Test {
 protected:
  void SetUp() override {
    // Set up users to be used in the sub-tests
    for(size_t i = 0; i < kNumOfUsersTotal; ++i) {
      user_list_.push_back(std::string("user") + std::to_string(i));
      if (i < kNumOfUsersPreset) {
        bool ok = service_data_structure_.UserRegister(user_list_[i]);
      }
    }
  }

  // The container to store the usernames
  std::vector<std::string> user_list_;

  // The object that this test is going to test
  ServiceDataStructure service_data_structure_;
};

// This tests on the `UserRegister` and `Login` in `ServiceDataStructure`
TEST_F(ServiceTestDataStructure, UserRegisterAndLoginTest) {
  // Try to register existed usernames which already done in the `SetUp()`
  for(size_t i = 0; i < kNumOfUsersPreset; ++i) {
    bool ok = service_data_structure_.UserRegister(user_list_[i]);
    // This should fail since the username specified has already been registered in the `SetUp` process above
    EXPECT_FALSE(ok);
  }

  // Try to register non-existed usernames
  for(size_t i = kNumOfUsersPreset; i < kNumOfUsersTotal; ++i) {
    bool ok = service_data_structure_.UserRegister(user_list_[i]);
    // This should succeed since the username specified is not registered
    EXPECT_TRUE(ok);
  }

  // Try to login to those usernames that have been registered
  for(size_t i = 0; i < kNumOfUsersTotal; ++i) {
    // std::unique_ptr<ServiceDataStructure::UserSession>
    auto session_1 = service_data_structure_.UserLogin(user_list_[i]);
    // std::unique_ptr<ServiceDataStructure::UserSession>
    auto session_2 = service_data_structure_.UserLogin(user_list_[i]);
    EXPECT_NE(nullptr, session_1);
    EXPECT_NE(nullptr, session_2);
    // Multiple logins are allowed
    EXPECT_NE(session_1, session_2);

    // Check username is identical
    EXPECT_EQ(user_list_[i], session_1->SessionGetUsername());
    EXPECT_EQ(user_list_[i], session_2->SessionGetUsername());
  }

  // Try to login to a non-existing username
  // std::unique_ptr<ServiceDataStructure::UserSession>
  auto session = service_data_structure_.UserLogin("nonexist");
  EXPECT_EQ(nullptr, session);
}

// This tests on the `Post`, `Edit`, and `Delete` in `ServiceDataStructure`
TEST_F(ServiceTestDataStructure, PostEditAndDeleteTest) {
  // The number of chirps posted
  const size_t kTestCase = 10;
  // The number of chirps which is going to be deleted
  const size_t kHalfTestCase = kTestCase / 2;

  // Set up expected contents for initial posts
  std::vector<std::string> chirps_content;
  for(size_t i = 0; i < kTestCase; ++i) {
    chirps_content.push_back(std::string("Chirp #") + std::to_string(i) + kShortText);
  }

  // Set up expected contents for posts after editing
  std::vector<std::string> chirps_content_after_edit(chirps_content);
  for(size_t i = 0; i < kTestCase; ++i) {
    chirps_content_after_edit[i] = std::string("Chirp #") + std::to_string(i) + kLongText;
  }

  // Set up expected contents for posts after deleting
  std::vector<std::string> chirps_content_after_delete(chirps_content_after_edit);
  for(size_t i = 0; i < kHalfTestCase; ++i) {
    chirps_content_after_delete.erase(chirps_content_after_delete.begin());
  }

  // Tests for every user
  for(size_t i = 0; i < kNumOfUsersPreset; ++i) {
    // std::unique_ptr<ServiceDataStructure::UserSession>
    auto session = service_data_structure_.UserLogin(user_list_[i]);
    std::vector<uint64_t> chirp_ids;

    // Login should be successful
    ASSERT_NE(nullptr, session);
    // Check the user_session is not broken
    EXPECT_EQ(user_list_[i], session->SessionGetUsername());

    // Test Posting
    uint64_t parent_id = 0;
    for(size_t j = 0; j < kTestCase; ++j) {
      uint64_t chirp_id = session->PostChirp(chirps_content[j], parent_id);
      // Posting should be successful
      ASSERT_NE(0, chirp_id);
      parent_id = chirp_id;

      // collect the chirp ids created
      chirp_ids.push_back(chirp_id);
    }

    // Read from backend
    // to see if the results are identical
    std::vector<std::string> chirps_content_from_backend;
    uint64_t last_id = 0;
    for(const auto& id : session->SessionGetUserChirpList()) {
      struct ServiceDataStructure::Chirp chirp;
      bool ok = service_data_structure_.ReadChirp(id, &chirp);
      // Reading should be successful
      ASSERT_TRUE(ok);
      EXPECT_EQ(last_id, chirp.parent_id);
      last_id = chirp.id;

      // colect the texts posted
      chirps_content_from_backend.push_back(chirp.text);
    }
    // Check if the contents are identical
    EXPECT_EQ(chirps_content, chirps_content_from_backend); 

    // Test Editing
    chirps_content_from_backend.clear();
    for(size_t j = 0; j < chirp_ids.size(); ++j) {
      auto ok = session->EditChirp(chirp_ids[j], chirps_content_after_edit[j]);
      // Editing should be successful
      ASSERT_TRUE(ok);
    }

    // Read from backend
    // to see if the results are identical
    for(auto& id : session->SessionGetUserChirpList()) {
      struct ServiceDataStructure::Chirp chirp;
      bool ok = service_data_structure_.ReadChirp(id, &chirp);
      // Reading should be successful
      ASSERT_TRUE(ok);

      // colect the texts posted
      chirps_content_from_backend.push_back(chirp.text);
    }
    // Check if the contents are identical
    EXPECT_EQ(chirps_content_after_edit, chirps_content_from_backend);

    // Test Deleting
    chirps_content_from_backend.clear();
    for(size_t j = 0; j < kHalfTestCase; ++j) {
      bool ok = session->DeleteChirp(chirp_ids[j]);
      // Deleting should be successful
      EXPECT_TRUE(ok);
      ok = session->DeleteChirp(chirp_ids[j]);
      // Deleting should be unsuccessful since this chirp is already deleted
      EXPECT_FALSE(ok);
    }

    // Read from backend
    // to see if the results are identical
    for(auto& id : session->SessionGetUserChirpList()) {
      struct ServiceDataStructure::Chirp chirp;
      bool ok = service_data_structure_.ReadChirp(id, &chirp);
      // Reading should be successful
      ASSERT_TRUE(ok);

      // colect the texts posted
      chirps_content_from_backend.push_back(chirp.text);
    }
    // Check if the contents are identical
    EXPECT_EQ(chirps_content_after_delete, chirps_content_from_backend);
  }
}

// This tests on `Follow` and `MonitorFrom` in `ServiceDataStructure`
TEST_F(ServiceTestDataStructure, FollowAndMonitorTest) {
  // Make each user follows the next user
  for(size_t i = 0; i < kNumOfUsersPreset; ++i) {
    // std::unique_ptr<ServiceDataStructure::UserSession>
    auto session = service_data_structure_.UserLogin(user_list_[i]);
    // Login should be successful
    EXPECT_NE(nullptr, session);
    bool ok = session->Follow(user_list_[(i + 1) % kNumOfUsersPreset]);
    // Following should be successful
    EXPECT_TRUE(ok);
    ok = session->Follow("non-existed");
    // Should not follow a non-existed user
    EXPECT_FALSE(ok);
  }

  // Each user monitors their following users
  for(size_t i = 0; i < kNumOfUsersPreset; ++i) {
    // std::unique_ptr<ServiceDataStructure::UserSession>
    auto session_user_followed = service_data_structure_.UserLogin(user_list_[(i + 1) % kNumOfUsersPreset]);
    // Login as the followed user should be successful
    EXPECT_NE(nullptr, session_user_followed);

    // Post some dont-care chirps from the followed user
    for(size_t j = 0; j < 5; ++j) {
      uint64_t chirp_id = session_user_followed->PostChirp(kShortText);
      // Posting should be successful
      EXPECT_NE(0, chirp_id);
    }

    // Timestamp the current time and back it up
    struct timeval now;
    gettimeofday(&now, nullptr);
    struct timeval backup_now = now;

    // sleep a little while to ensure that the time has passed at least 1 usec.
    std::this_thread::sleep_for(std::chrono::microseconds(1));

    // Collect the chirp ids after the above timestamp
    std::set<uint64_t> chirp_collector;
    for(size_t j = 0; j < 5; ++j) {
      auto chirp_id = session_user_followed->PostChirp(kShortText);
      // Posting should be successful
      EXPECT_NE(0, chirp_id);
      chirp_collector.insert(chirp_id);
    }

    // Test Monitoring from
    // std::unique_ptr<ServiceDataStructure::UserSession>
    auto session = service_data_structure_.UserLogin(user_list_[i]);
    // Login should be successful
    EXPECT_NE(nullptr, session);
    auto monitor_result = session->MonitorFrom(&now);
    // `now` should be modified by the `Monitor` function
    // so it should be different from the one I have backed up
    EXPECT_TRUE(timercmp(&backup_now, &now, !=));
    // Check if the contents are identical
    EXPECT_EQ(chirp_collector, monitor_result);
  }
}

// This test cases on the Service Server to check whether their
// interfaces work correctly.
class ServiceTestServer : public ::testing::Test {
 protected:
  void SetUp() override {
    // Set up users to be used in the sub-tests
    for (size_t i = 0; i < kNumOfUsersTotal; ++i) {
      user_list_.push_back(std::string("User") + std::to_string(i));

      if (i < kNumOfUsersPreset) {
        bool ok = service_client_.SendRegisterUserRequest(user_list_[i]);
      }
    }
  }

  std::vector<std::string> user_list_;
  ServiceClient service_client_;
};

// This tests on `registeruser` in Service Server
TEST_F(ServiceTestServer, RegisterUser) {
  // Try to register existed usernames
  for(size_t i = 0; i < kNumOfUsersPreset; ++i) {
    bool ok = service_client_.SendRegisterUserRequest(user_list_[i]);
    // This should fail since the username specified has already been registered in the `SetUp` process above
    EXPECT_FALSE(ok) << "This should fail since the username specified has been registered.";
  }

  // Try to register non-existed usernames
  for(size_t i = kNumOfUsersPreset; i < kNumOfUsersTotal; ++i) {
    bool ok = service_client_.SendRegisterUserRequest(user_list_[i]);
    // This should succeed since the username specified is not registered
    EXPECT_TRUE(ok) << "This registration should succeed.";
  }

  // Try to register all usernames again
  for(size_t i = 0; i < kNumOfUsersTotal; ++i) {
    bool ok = service_client_.SendRegisterUserRequest(user_list_[i]);
    // This should fail since all these usernames are already registered
    EXPECT_FALSE(ok) << "This should fail since the username specified has been registered.";
  }
}

// This tests on `chirp` in Service Server
TEST_F(ServiceTestServer, Chirp) {
  // Every user posts a chirp
  uint64_t last_id = 0;
  for (size_t i = 0; i < kNumOfUsersTotal; ++i) {
    struct ServiceClient::Chirp chirp;
    bool ok = service_client_.SendChirpRequest(user_list_[i], kShortText, last_id, &chirp);
    // The three lines below check if the data in a chirp data structure corresponds to
    // the data provided above
    EXPECT_EQ(user_list_[i], chirp.username);
    EXPECT_EQ(kShortText, chirp.text);
    EXPECT_EQ(last_id, chirp.parent_id);
    last_id = chirp.id;
  }
}

// This tests on `follow` in Service Server
TEST_F(ServiceTestServer, Follow) {
  // Every user follows its next user
  for(size_t i = 0; i < kNumOfUsersTotal; ++i) {
    bool ok = service_client_.SendFollowRequest(user_list_[i], user_list_[(i + 1) % kNumOfUsersTotal]);
    // This should be successful
    EXPECT_TRUE(ok);
    // This should be unsuccessful since it tries to follow a non-existed user
    ok = service_client_.SendFollowRequest(user_list_[i], "non-existed");
    EXPECT_FALSE(ok);
  }
}

// This tests on `read` in Service Server
TEST_F(ServiceTestServer, Read) {
  // The container of chirp ids
  std::vector<uint64_t> corrected_chirps;

  for(size_t i = 0; i < kNumOfUsersTotal; ++i) {
    corrected_chirps.clear();
    ServiceClient::Chirp chirp;

    // In the following three layers, I try to build a nested chirps like this
    // #01 - #02 - #05
    //           - #06
    //           - #07
    //     - #03
    //     - #04

    // Layer 1
    bool ok = service_client_.SendChirpRequest(user_list_[i], kShortText, 0, &chirp);
    // Posting a chirp should be successful
    EXPECT_TRUE(ok);
    corrected_chirps.push_back(chirp.id);

    // Layer 2
    for (size_t j = 0; j < 3; ++j) {
      ok = service_client_.SendChirpRequest(user_list_[i], kShortText, corrected_chirps[0], &chirp);
      // Posting a chirp should be successful
      EXPECT_TRUE(ok);
      corrected_chirps.push_back(chirp.id);
    }

    // Layer 3
    std::vector<uint64_t> tmp;
    for(size_t j = 0; j < 3; ++j) {
      ok = service_client_.SendChirpRequest(user_list_[i], kShortText, corrected_chirps[1], &chirp);
      // Posting a chirp should be successful
      EXPECT_TRUE(ok);
      tmp.push_back(chirp.id);
    }
    corrected_chirps.insert(corrected_chirps.begin() + 2, tmp.begin(), tmp.end());

    // Prepare to read
    std::vector<struct ServiceClient::Chirp> reply;
    ok = service_client_.SendReadRequest(corrected_chirps[0], &reply);
    // Reading should be successful
    EXPECT_TRUE(ok);
    // Check if the ids from reading and the ids in the container are identical
    for(size_t j = 0; j < corrected_chirps.size(); ++j) {
      EXPECT_EQ(user_list_[i], reply[j].username);
      EXPECT_EQ(corrected_chirps[j], reply[j].id);
    }
  }
}

// This tests on `monitor` in Service Server
TEST_F(ServiceTestServer, Monitor) {
  // Make the last user to follow all other users
  for(size_t i = 0; i < kNumOfUsersTotal - 1; ++i) {
    // Don't care about the return value of this
    service_client_.SendFollowRequest(user_list_.back(), user_list_[i]);
  }

  // Container of chirp ids
  std::vector<uint64_t> chirpids;

  // Keep posting chirps in another thread
  std::thread posting_chirps([&]() {
    // Every user takes turn to post
    for(size_t i = 0; i < kNumOfUsersTotal - 1; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      ServiceClient::Chirp chirp;
      bool ok = service_client_.SendChirpRequest(user_list_[i], kShortText, 0, &chirp);
      // Posting chirps should be successful
      EXPECT_TRUE(ok);
      chirpids.push_back(chirp.id);
    }
  });

  // Send `monitor` request simultaneously
  std::vector<ServiceClient::Chirp> chirps;
  service_client_.SendMonitorRequest(user_list_.back(), &chirps);

  // Wait for the thread to finish
  posting_chirps.join();

  // Check if the results are identical
  for(size_t i = 0; i < chirpids.size(); ++i) {
    EXPECT_EQ(chirpids[i], chirps[i].id) << " i is " << i << std::endl;
  }
}

} // end of namespace

GTEST_API_ int main(int argc, char **argv) {
  // glog initialization
  FLAGS_log_dir = "./log";
  google::InitGoogleLogging(argv[0]);

  LOG(INFO) << "glog on testing starts.";

  std::cout << "Running service tests from " << __FILE__ << std::endl;
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}