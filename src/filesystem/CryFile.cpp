#include "CryFile.h"

#include "CryDevice.h"
#include "CryOpenFile.h"
#include "messmer/fspp/fuse/FuseErrnoException.h"

namespace bf = boost::filesystem;

//TODO Get rid of this in favor of exception hierarchy
using fspp::fuse::CHECK_RETVAL;
using fspp::fuse::FuseErrnoException;

using blockstore::Key;
using boost::none;
using cpputils::unique_ref;
using cpputils::make_unique_ref;
using cpputils::dynamic_pointer_move;
using cryfs::parallelaccessfsblobstore::DirBlobRef;
using cryfs::parallelaccessfsblobstore::FileBlobRef;

namespace cryfs {

CryFile::CryFile(CryDevice *device, unique_ref<DirBlobRef> parent, const Key &key)
: CryNode(device, std::move(parent), key) {
}

CryFile::~CryFile() {
}

unique_ref<parallelaccessfsblobstore::FileBlobRef> CryFile::LoadBlob() const {
  auto blob = CryNode::LoadBlob();
  auto file_blob = dynamic_pointer_move<FileBlobRef>(blob);
  ASSERT(file_blob != none, "Blob does not store a file");
  return std::move(*file_blob);
}

unique_ref<fspp::OpenFile> CryFile::open(int flags) const {
  device()->callFsActionCallbacks();
  auto blob = LoadBlob();
  return make_unique_ref<CryOpenFile>(device(), std::move(blob));
}

void CryFile::truncate(off_t size) const {
  device()->callFsActionCallbacks();
  auto blob = LoadBlob();
  blob->resize(size);
}

fspp::Dir::EntryType CryFile::getType() const {
  device()->callFsActionCallbacks();
  return fspp::Dir::EntryType::FILE;
}

}
