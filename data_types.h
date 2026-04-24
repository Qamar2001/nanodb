// data_types.h - polymorphic column values
#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <string>
#include <fstream>

enum TypeTag { T_INT = 1, T_FLOAT = 2, T_STRING = 3 };

// base class for all column values
class DataType {
public:
    virtual ~DataType() {}
    virtual TypeTag getType() const = 0;
    virtual DataType* clone() const = 0;
    virtual std::string toString() const = 0;
    virtual void serialize(std::ofstream& out) const = 0;

    // polymorphic comparison operators. derived types check type-compat.
    virtual bool operator==(const DataType& other) const = 0;
    virtual bool operator<(const DataType& other) const = 0;
    virtual bool operator>(const DataType& other) const = 0;
    bool operator!=(const DataType& o) const { return !(*this == o); }
    bool operator<=(const DataType& o) const { return (*this < o) || (*this == o); }
    bool operator>=(const DataType& o) const { return (*this > o) || (*this == o); }

    // numeric helpers - used by arithmetic in parser (multiply, mod, etc.)
    virtual double asNumber() const = 0;
    virtual bool isNumeric() const = 0;

    // factory: reads a tag byte then rebuilds the proper subclass
    static DataType* deserialize(std::ifstream& in);
};

// ---- IntType ----
class IntType : public DataType {
public:
    int value;
    IntType(int v = 0) : value(v) {}
    TypeTag getType() const { return T_INT; }
    DataType* clone() const { return new IntType(value); }
    std::string toString() const { return std::to_string(value); }
    void serialize(std::ofstream& out) const;
    bool operator==(const DataType& o) const;
    bool operator<(const DataType& o) const;
    bool operator>(const DataType& o) const;
    double asNumber() const { return (double)value; }
    bool isNumeric() const { return true; }
};

// ---- FloatType ----
class FloatType : public DataType {
public:
    double value;
    FloatType(double v = 0.0) : value(v) {}
    TypeTag getType() const { return T_FLOAT; }
    DataType* clone() const { return new FloatType(value); }
    std::string toString() const { return std::to_string(value); }
    void serialize(std::ofstream& out) const;
    bool operator==(const DataType& o) const;
    bool operator<(const DataType& o) const;
    bool operator>(const DataType& o) const;
    double asNumber() const { return value; }
    bool isNumeric() const { return true; }
};

// ---- StringType ----
class StringType : public DataType {
public:
    std::string value;
    StringType(const std::string& v = "") : value(v) {}
    TypeTag getType() const { return T_STRING; }
    DataType* clone() const { return new StringType(value); }
    std::string toString() const { return value; }
    void serialize(std::ofstream& out) const;
    bool operator==(const DataType& o) const;
    bool operator<(const DataType& o) const;
    bool operator>(const DataType& o) const;
    double asNumber() const { return 0.0; }
    bool isNumeric() const { return false; }
};

#endif
