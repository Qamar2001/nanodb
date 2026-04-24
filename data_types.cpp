// data_types.cpp
#include "data_types.h"
#include <cstdint>

// ----------- IntType -----------
void IntType::serialize(std::ofstream& out) const {
    uint8_t tag = (uint8_t)T_INT;
    out.write((char*)&tag, 1);
    out.write((char*)&value, sizeof(int));
}
bool IntType::operator==(const DataType& o) const {
    if (o.getType() == T_INT)   return value == ((IntType&)o).value;
    if (o.getType() == T_FLOAT) return (double)value == ((FloatType&)o).value;
    return false;
}
bool IntType::operator<(const DataType& o) const {
    if (o.getType() == T_INT)   return value < ((IntType&)o).value;
    if (o.getType() == T_FLOAT) return (double)value < ((FloatType&)o).value;
    return false;
}
bool IntType::operator>(const DataType& o) const {
    if (o.getType() == T_INT)   return value > ((IntType&)o).value;
    if (o.getType() == T_FLOAT) return (double)value > ((FloatType&)o).value;
    return false;
}

// ----------- FloatType -----------
void FloatType::serialize(std::ofstream& out) const {
    uint8_t tag = (uint8_t)T_FLOAT;
    out.write((char*)&tag, 1);
    out.write((char*)&value, sizeof(double));
}
bool FloatType::operator==(const DataType& o) const {
    if (o.getType() == T_FLOAT) return value == ((FloatType&)o).value;
    if (o.getType() == T_INT)   return value == (double)((IntType&)o).value;
    return false;
}
bool FloatType::operator<(const DataType& o) const {
    if (o.getType() == T_FLOAT) return value < ((FloatType&)o).value;
    if (o.getType() == T_INT)   return value < (double)((IntType&)o).value;
    return false;
}
bool FloatType::operator>(const DataType& o) const {
    if (o.getType() == T_FLOAT) return value > ((FloatType&)o).value;
    if (o.getType() == T_INT)   return value > (double)((IntType&)o).value;
    return false;
}

// ----------- StringType -----------
void StringType::serialize(std::ofstream& out) const {
    uint8_t tag = (uint8_t)T_STRING;
    out.write((char*)&tag, 1);
    uint32_t len = (uint32_t)value.size();
    out.write((char*)&len, sizeof(uint32_t));
    if (len) out.write(value.data(), len);
}
bool StringType::operator==(const DataType& o) const {
    if (o.getType() == T_STRING) return value == ((StringType&)o).value;
    return false;
}
bool StringType::operator<(const DataType& o) const {
    if (o.getType() == T_STRING) return value < ((StringType&)o).value;
    return false;
}
bool StringType::operator>(const DataType& o) const {
    if (o.getType() == T_STRING) return value > ((StringType&)o).value;
    return false;
}

// ----------- deserialize factory -----------
DataType* DataType::deserialize(std::ifstream& in) {
    uint8_t tag = 0;
    in.read((char*)&tag, 1);
    if (!in) return nullptr;
    if (tag == T_INT) {
        int v; in.read((char*)&v, sizeof(int));
        return new IntType(v);
    }
    if (tag == T_FLOAT) {
        double v; in.read((char*)&v, sizeof(double));
        return new FloatType(v);
    }
    if (tag == T_STRING) {
        uint32_t len = 0;
        in.read((char*)&len, sizeof(uint32_t));
        std::string s;
        if (len) { s.resize(len); in.read(&s[0], len); }
        return new StringType(s);
    }
    return nullptr;
}
