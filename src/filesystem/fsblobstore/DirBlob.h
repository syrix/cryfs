#pragma once
#ifndef MESSMER_CRYFS_FILESYSTEM_FSBLOBSTORE_DIRBLOB_H_
#define MESSMER_CRYFS_FILESYSTEM_FSBLOBSTORE_DIRBLOB_H_

#include <messmer/blockstore/utils/Key.h>
#include <messmer/cpp-utils/macros.h>
#include <messmer/fspp/fs_interface/Dir.h>
#include "FsBlob.h"
#include "utils/DirEntryList.h"
#include <mutex>

namespace cryfs {
    namespace fsblobstore {
        class FsBlobStore;

        class DirBlob final : public FsBlob {
        public:

            static cpputils::unique_ref<DirBlob> InitializeEmptyDir(cpputils::unique_ref<blobstore::Blob> blob,
                                                                    std::function<off_t (const blockstore::Key&)> getLstatSize);

            DirBlob(cpputils::unique_ref<blobstore::Blob> blob, std::function<off_t (const blockstore::Key&)> getLstatSize);

            ~DirBlob();

            off_t lstat_size() const override;

            void AppendChildrenTo(std::vector<fspp::Dir::Entry> *result) const;

            const DirEntry &GetChild(const std::string &name) const;

            const DirEntry &GetChild(const blockstore::Key &key) const;

            void AddChildDir(const std::string &name, const blockstore::Key &blobKey, mode_t mode, uid_t uid,
                             gid_t gid);

            void AddChildFile(const std::string &name, const blockstore::Key &blobKey, mode_t mode, uid_t uid,
                              gid_t gid);

            void AddChildSymlink(const std::string &name, const blockstore::Key &blobKey, uid_t uid, gid_t gid);

            void AddChild(const std::string &name, const blockstore::Key &blobKey, fspp::Dir::EntryType type,
                          mode_t mode,
                          uid_t uid, gid_t gid);

            void RemoveChild(const blockstore::Key &key);

            void flush();

            void statChild(const blockstore::Key &key, struct ::stat *result) const;

            void chmodChild(const blockstore::Key &key, mode_t mode);

            void chownChild(const blockstore::Key &key, uid_t uid, gid_t gid);

            void setLstatSizeGetter(std::function<off_t(const blockstore::Key&)> getLstatSize);

        private:

            void _readEntriesFromBlob();
            void _writeEntriesToBlob();

            cpputils::unique_ref<blobstore::Blob> releaseBaseBlob() override;

            std::function<off_t (const blockstore::Key&)> _getLstatSize;
            DirEntryList _entries;
            mutable std::mutex _mutex;
            bool _changed;

            DISALLOW_COPY_AND_ASSIGN(DirBlob);
        };

    }
}

#endif
