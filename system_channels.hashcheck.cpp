/* C++ code produced by gperf version 3.3-beta */
/* Command-line: gperf.exe --output-file='Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.hashcheck.cpp' 'Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf' */
/* Computed positions: -k'1,9' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 1 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#line 14 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
struct ChannelIdentifier {
  const char *name;
  int channel;
};
enum
  {
    TOTAL_KEYWORDS = 15,
    MIN_WORD_LENGTH = 1,
    MAX_WORD_LENGTH = 11,
    MIN_HASH_VALUE = 1,
    MAX_HASH_VALUE = 24
  };

/* maximum key range = 24, duplicates = 0 */

class SystemChannelRecognizer
{
private:
  static inline unsigned int hash (const char *str, size_t len);
public:
  static const struct ChannelIdentifier *in_word_set (const char *str, size_t len);
};

inline unsigned int
SystemChannelRecognizer::hash (const char *str, size_t len)
{
  static const unsigned char asso_values[] =
    {
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25,  9, 25,  4, 25,  0, 25,  5,
       0, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 15, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 10, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25,  5,
      25,  5, 25, 25, 25, 25, 25, 25, 15, 25,
       0,  0, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25
    };
  unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[static_cast<unsigned char>(str[8])];
#if (defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang__ && __clang_major__ + (__clang_minor__ >= 9) > 3))) || (__STDC_VERSION__ >= 202000L && ((defined __GNUC__ && __GNUC__ >= 10) || (defined __clang__ && __clang_major__ >= 9)))
      [[fallthrough]];
#elif (defined __GNUC__ && __GNUC__ >= 7) || (defined __clang__ && __clang_major__ >= 10)
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 8:
      case 7:
      case 6:
      case 5:
      case 4:
      case 3:
      case 2:
      case 1:
        hval += asso_values[static_cast<unsigned char>(str[0])];
        break;
    }
  return hval;
}

static const struct ChannelIdentifier wordlist[] =
  {
    {(char*)00},
#line 20 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"2",								 2},
    {(char*)00},
#line 21 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"nul",							 3},
#line 23 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"nul:",							 3},
#line 29 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"-",								 1},
#line 19 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"1",								 1},
    {(char*)00},
#line 25 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"con",							 2},
#line 27 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"con:",							 2},
#line 30 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"+",								 2},
#line 31 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"/dev/stdout",					 1},
    {(char*)00},
#line 22 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"NUL",							 3},
#line 24 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"NUL:",							 3},
    {(char*)00},
#line 32 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"/dev/stderr",					 2},
    {(char*)00},
#line 26 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"CON",							 2},
#line 28 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"CON:",							 2},
    {(char*)00}, {(char*)00}, {(char*)00}, {(char*)00},
#line 33 "Z:\\lib\\tooling\\qiqqa\\MuPDF\\platform\\win32\\../../thirdparty/owemdjee/libpathutils/system_channels.gperf"
    {"/dev/null",						 3}
  };

const struct ChannelIdentifier *
SystemChannelRecognizer::in_word_set (const char *str, size_t len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      unsigned int key = hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          const char *s = wordlist[key].name;

          if (s && *str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
