#ifndef MYSQL_TYPES_H
#define MYSQL_TYPES_H

using TableName = const char*;

class ByteData {
 public:
  ByteData() {}
  ByteData(unsigned char *p_data, uint8_t p_dataSize) {
    data = p_data;
    dataSize = p_dataSize;
  }

  unsigned char *data;
  uint8_t dataSize;
};

class PutOp {
 public:
  TableName table;
  std::unique_ptr<ByteData> value;
  std::unique_ptr<ByteData> key;

  PutOp() {
    value = std::make_unique<ByteData>();
    key = std::make_unique<ByteData>();
  }

  ~PutOp() {
    free(key->data);
    free(value->data);
  }
};


class RemoveOp {
 public:
  TableName table;
  std::unique_ptr<ByteData> key;

  RemoveOp() {
    key = std::make_unique<ByteData>();
  }

  ~RemoveOp() {
    free(key->data);
  }
};

#endif  // MYSQL_TYPES_H
