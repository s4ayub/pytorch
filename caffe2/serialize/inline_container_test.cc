#include <cstdio>
#include <string>
#include <array>

#include <gtest/gtest.h>

#include "caffe2/serialize/inline_container.h"
#include "c10/util/irange.h"
#include <c10/util/uuid.h>

namespace caffe2 {
namespace serialize {
namespace {

TEST(PyTorchStreamWriterAndReader, SaveAndLoad) {
  int64_t kFieldAlignment = 64L;

  std::ostringstream oss;
  // write records through writers
  PyTorchStreamWriter writer([&](const void* b, size_t n) -> size_t {
    oss.write(static_cast<const char*>(b), n);
    return oss ? n : 0;
  });
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,cppcoreguidelines-avoid-magic-numbers)
  std::array<char, 127> data1;

  for (auto i: c10::irange( data1.size())) {
    data1[i] = data1.size() - i;
  }
  writer.writeRecord("key1", data1.data(), data1.size());

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,cppcoreguidelines-avoid-magic-numbers)
  std::array<char, 64> data2;
  for (auto i: c10::irange(data2.size())) {
    data2[i] = data2.size() - i;
  }
  writer.writeRecord("key2", data2.data(), data2.size());

  const std::unordered_set<std::string>& written_records =
      writer.getAllWrittenRecords();
  ASSERT_EQ(written_records.size(), 2);
  ASSERT_EQ(written_records.count("key1"), 1);
  ASSERT_EQ(written_records.count("key2"), 1);

  writer.writeEndOfFile();
  ASSERT_EQ(written_records.count(kSerializationIdRecordName), 1);

  std::string the_file = oss.str();
  std::ofstream foo("output.zip");
  foo.write(the_file.c_str(), the_file.size());
  foo.close();

  std::istringstream iss(the_file);

  // read records through readers
  PyTorchStreamReader reader(&iss);
  ASSERT_TRUE(reader.hasRecord("key1"));
  ASSERT_TRUE(reader.hasRecord("key2"));
  ASSERT_FALSE(reader.hasRecord("key2000"));
  at::DataPtr data_ptr;
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  int64_t size;
  std::tie(data_ptr, size) = reader.getRecord("key1");
  size_t off1 = reader.getRecordOffset("key1");
  ASSERT_EQ(size, data1.size());
  ASSERT_EQ(memcmp(data_ptr.get(), data1.data(), data1.size()), 0);
  ASSERT_EQ(memcmp(the_file.c_str() + off1, data1.data(), data1.size()), 0);
  ASSERT_EQ(off1 % kFieldAlignment, 0);
  // inplace getRecord() test
  std::vector<uint8_t> dst(size);
  size_t ret = reader.getRecord("key1", dst.data(), size);
  ASSERT_EQ(ret, size);
  ASSERT_EQ(memcmp(dst.data(), data1.data(), size), 0);
  // chunked getRecord() test
  ret = reader.getRecord(
      "key1", dst.data(), size, 3, [](void* dst, const void* src, size_t n) {
        memcpy(dst, src, n);
      });
  ASSERT_EQ(ret, size);
  ASSERT_EQ(memcmp(dst.data(), data1.data(), size), 0);

  std::tie(data_ptr, size) = reader.getRecord("key2");
  size_t off2 = reader.getRecordOffset("key2");
  ASSERT_EQ(off2 % kFieldAlignment, 0);

  ASSERT_EQ(size, data2.size());
  ASSERT_EQ(memcmp(data_ptr.get(), data2.data(), data2.size()), 0);
  ASSERT_EQ(memcmp(the_file.c_str() + off2, data2.data(), data2.size()), 0);
  // inplace getRecord() test
  dst.resize(size);
  ret = reader.getRecord("key2", dst.data(), size);
  ASSERT_EQ(ret, size);
  ASSERT_EQ(memcmp(dst.data(), data2.data(), size), 0);
  // chunked getRecord() test
  ret = reader.getRecord(
      "key2", dst.data(), size, 3, [](void* dst, const void* src, size_t n) {
        memcpy(dst, src, n);
      });
  ASSERT_EQ(ret, size);
  ASSERT_EQ(memcmp(dst.data(), data2.data(), size), 0);
}

TEST(PytorchStreamWriterAndReader, GetNonexistentRecordThrows) {
  std::ostringstream oss;
  // write records through writers
  PyTorchStreamWriter writer([&](const void* b, size_t n) -> size_t {
    oss.write(static_cast<const char*>(b), n);
    return oss ? n : 0;
  });
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,cppcoreguidelines-avoid-magic-numbers)
  std::array<char, 127> data1;

  for (auto i: c10::irange(data1.size())) {
    data1[i] = data1.size() - i;
  }
  writer.writeRecord("key1", data1.data(), data1.size());

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,cppcoreguidelines-avoid-magic-numbers)
  std::array<char, 64> data2;
  for (auto i: c10::irange(data2.size())) {
    data2[i] = data2.size() - i;
  }
  writer.writeRecord("key2", data2.data(), data2.size());

  const std::unordered_set<std::string>& written_records =
      writer.getAllWrittenRecords();
  ASSERT_EQ(written_records.size(), 2);
  ASSERT_EQ(written_records.count("key1"), 1);
  ASSERT_EQ(written_records.count("key2"), 1);

  writer.writeEndOfFile();
  ASSERT_EQ(written_records.count(kSerializationIdRecordName), 1);

  std::string the_file = oss.str();
  std::ofstream foo("output2.zip");
  foo.write(the_file.c_str(), the_file.size());
  foo.close();

  std::istringstream iss(the_file);

  // read records through readers
  PyTorchStreamReader reader(&iss);
  // NOLINTNEXTLINE(hicpp-avoid-goto,cppcoreguidelines-avoid-goto)
  EXPECT_THROW(reader.getRecord("key3"), c10::Error);
  std::vector<uint8_t> dst(data1.size());
  EXPECT_THROW(reader.getRecord("key3", dst.data(), data1.size()), c10::Error);
  EXPECT_THROW(
      reader.getRecord(
          "key3",
          dst.data(),
          data1.size(),
          3,
          [](void* dst, const void* src, size_t n) { memcpy(dst, src, n); }),
      c10::Error);

  // Reader should still work after throwing
  EXPECT_TRUE(reader.hasRecord("key1"));
}

TEST(PytorchStreamWriterAndReader, SkipDebugRecords) {
  std::ostringstream oss;
  PyTorchStreamWriter writer([&](const void* b, size_t n) -> size_t {
    oss.write(static_cast<const char*>(b), n);
    return oss ? n : 0;
  });
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,cppcoreguidelines-avoid-magic-numbers)
  std::array<char, 127> data1;

  for (auto i: c10::irange(data1.size())) {
    data1[i] = data1.size() - i;
  }
  writer.writeRecord("key1.debug_pkl", data1.data(), data1.size());

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,cppcoreguidelines-avoid-magic-numbers)
  std::array<char, 64> data2;
  for (auto i: c10::irange(data2.size())) {
    data2[i] = data2.size() - i;
  }
  writer.writeRecord("key2.debug_pkl", data2.data(), data2.size());

  const std::unordered_set<std::string>& written_records =
      writer.getAllWrittenRecords();
  ASSERT_EQ(written_records.size(), 2);
  ASSERT_EQ(written_records.count("key1.debug_pkl"), 1);
  ASSERT_EQ(written_records.count("key2.debug_pkl"), 1);
  writer.writeEndOfFile();
  ASSERT_EQ(written_records.count(kSerializationIdRecordName), 1);

  std::string the_file = oss.str();
  std::ofstream foo("output2.zip");
  foo.write(the_file.c_str(), the_file.size());
  foo.close();

  std::istringstream iss(the_file);

  // read records through readers
  PyTorchStreamReader reader(&iss);
  // NOLINTNEXTLINE(hicpp-avoid-goto,cppcoreguidelines-avoid-goto)

  reader.setShouldLoadDebugSymbol(false);
  EXPECT_FALSE(reader.hasRecord("key1.debug_pkl"));
  at::DataPtr ptr;
  size_t size;
  std::tie(ptr, size) = reader.getRecord("key1.debug_pkl");
  EXPECT_EQ(size, 0);
  std::vector<uint8_t> dst(data1.size());
  size_t ret = reader.getRecord("key1.debug_pkl", dst.data(), data1.size());
  EXPECT_EQ(ret, 0);
  ret = reader.getRecord(
      "key1.debug_pkl",
      dst.data(),
      data1.size(),
      3,
      [](void* dst, const void* src, size_t n) { memcpy(dst, src, n); });
  EXPECT_EQ(ret, 0);
}

TEST(PytorchStreamWriterAndReader, SkipDuplicateSerializationIdRecords) {
  std::ostringstream oss;
  PyTorchStreamWriter writer([&](const void* b, size_t n) -> size_t {
    oss.write(static_cast<const char*>(b), n);
    return oss ? n : 0;
  });

  auto writer_serialization_id = writer.serializationId();
  auto dup_serialization_id = uuid::generate_uuid_v4();
  writer.writeRecord(kSerializationIdRecordName, dup_serialization_id.c_str(), dup_serialization_id.size());

  const std::unordered_set<std::string>& written_records =
      writer.getAllWrittenRecords();
  ASSERT_EQ(written_records.size(), 0);
  writer.writeEndOfFile();
  ASSERT_EQ(written_records.count(kSerializationIdRecordName), 1);

  std::string the_file = oss.str();
  std::ofstream foo("output3.zip");
  foo.write(the_file.c_str(), the_file.size());
  foo.close();

  std::istringstream iss(the_file);

  // read records through readers
  PyTorchStreamReader reader(&iss);
  // NOLINTNEXTLINE(hicpp-avoid-goto,cppcoreguidelines-avoid-goto)

  EXPECT_EQ(reader.serializationId(), writer_serialization_id);
}

} // namespace
} // namespace serialize
} // namespace caffe2
