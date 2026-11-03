#ifndef SMARTTREE_COMMAND_PARSER_H
#define SMARTTREE_COMMAND_PARSER_H
#include <stddef.h>
class CommandParser {
public:
    enum Result { WAITING, LINE, TOO_LONG };
    CommandParser() : length_(0),discard_(false) { line_[0]='\0'; }
    Result feed(char c) {
        if (c=='\r') return WAITING;
        if (c=='\n') {
            line_[length_]='\0';
            Result result=discard_ ? TOO_LONG : LINE;
            length_=0; discard_=false; return result;
        }
        if (!discard_) {
            if (length_<sizeof(line_)-1) line_[length_++]=c;
            else discard_=true;
        }
        return WAITING;
    }
    const char* line() const { return line_; }
private:
    char line_[48];
    size_t length_;
    bool discard_;
};
#endif
