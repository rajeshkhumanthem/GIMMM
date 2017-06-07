#ifndef MACROS_H
#define MACROS_H


#define THROW_INVALID_ARGUMENT_EXCEPTION(errmsg) \
  std::stringstream out; \
  out << "Exception @ [File:" << __FILE__ \
      << ", Line:" << __LINE__ << ", errormsg:" \
      << errmsg << "]"; throw std::invalid_argument(out.str().c_str())

#define PRINT_EXCEPTION_STRING(out, ex) \
  out << "Exception caught @ [FILE:" << __FILE__ \
      << ", Line:" << __LINE__ << "], What:" \
  << ex.what() << std::endl

#define PRINT_JSON_DOC(out, jsondoc) \
    QString str = jsondoc.toJson(QJsonDocument::Indented); \
    out << str.toStdString() << std::endl;

#define FCM_TAG(id) \
    "RX[" << id << "]:"


#endif // MACROS_H
