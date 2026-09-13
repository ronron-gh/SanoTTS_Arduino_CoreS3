#include "text_input.h"
#include <cassert>
#include <cstring>
int main(){
 TextLine l;
 for(const char *p="hello\r\nworld\n";*p;p++){auto r=l.push(*p);if(*p=='\r')assert(r==TextLine::Complete);}
 assert(!validText("\xc0\xaf",2));assert(!validText("\xed\xa0\x80",3));assert(!validText("\xf4\x90\x80\x80",4));
 assert(!validText("\xe3\x81",2));assert(!validText("a\0b",3));assert(!validText("   ",3));
 assert(validText("今日は良い天気ですね。",strlen("今日は良い天気ですね。")));
 l.reset();for(int i=0;i<1023;i++)assert(l.push('a')==TextLine::More);assert(l.push('\n')==TextLine::Complete);
 for(int i=0;i<1024;i++)l.push('a');assert(l.push('\r')==TextLine::Rejected);assert(l.push('\n')==TextLine::More);
 l.push('b');assert(l.push('\n')==TextLine::Complete);assert(!strcmp(l.text(),"b"));
 assert(l.push('\n')==TextLine::Rejected);
}
