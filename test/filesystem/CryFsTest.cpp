#include <google/gtest/gtest.h>
#include <messmer/cpp-utils/tempfile/TempDir.h>
#include <messmer/cpp-utils/tempfile/TempFile.h>
#include <messmer/cpp-utils/pointer/cast.h>
#include <messmer/blockstore/implementations/ondisk/OnDiskBlockStore.h>
#include "../../src/filesystem/CryDevice.h"
#include "../../src/filesystem/CryDir.h"
#include "../../src/filesystem/CryFile.h"
#include "../../src/filesystem/CryOpenFile.h"
#include "../testutils/MockConsole.h"
#include "../../src/config/CryConfigLoader.h"

//TODO (whole project) Make constructors explicit when implicit construction not needed

using ::testing::Test;
using ::testing::Return;
using ::testing::_;
using cpputils::TempDir;
using cpputils::TempFile;
using cpputils::dynamic_pointer_move;
using cpputils::make_unique_ref;
using cpputils::unique_ref;
using cpputils::Console;
using cpputils::Random;
using cpputils::SCrypt;
using cpputils::Data;
using blockstore::ondisk::OnDiskBlockStore;
using boost::none;

namespace bf = boost::filesystem;
using namespace cryfs;

class CryFsTest: public Test, public TestWithMockConsole {
public:
  CryFsTest(): rootdir(), config(false) {
  }

  CryConfigFile loadOrCreateConfig() {
    auto askPassword = [] {return "mypassword";};
    return CryConfigLoader(mockConsole(), Random::PseudoRandom(), SCrypt::TestSettings, askPassword, askPassword, none).loadOrCreate(config.path()).value();
  }

  unique_ref<OnDiskBlockStore> blockStore() {
    return make_unique_ref<OnDiskBlockStore>(rootdir.path());
  }

  TempDir rootdir;
  TempFile config;
};

TEST_F(CryFsTest, CreatedRootdirIsLoadableAfterClosing) {
  {
    CryDevice dev(loadOrCreateConfig(), blockStore());
  }
  CryDevice dev(loadOrCreateConfig(), blockStore());
  auto root = dev.Load(bf::path("/"));
  dynamic_pointer_move<CryDir>(root.get()).get()->children();
}

TEST_F(CryFsTest, LoadingFilesystemDoesntModifyConfigFile) {
  {
    CryDevice dev(loadOrCreateConfig(), blockStore());
  }
  Data configAfterCreating = Data::LoadFromFile(config.path()).value();
  {
    CryDevice dev(loadOrCreateConfig(), blockStore());
  }
  Data configAfterLoading = Data::LoadFromFile(config.path()).value();
  EXPECT_EQ(configAfterCreating, configAfterLoading);
}
