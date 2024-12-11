#include <stdio.h>
#include <iconv.h>
#include <errno.h>
#include <string.h>

/* Tests what iconv does with non-representable characters during conversion,
   whether they are replaced, discarded or whether the conversion fails with
   eilseq.

   Usage:
   source_encoding target_encoding [--enable-transliteration
                                    --disable-transliteration
                                    --enable-ilseq-invalid
                                    --disable-ilseq-invalid]
   
   Transliteration and ilseq-invalid is enabled/disabled using iconvctl when
   USE_ICONVCTL is defined (below).
*/

#ifdef __APPLE__
# define USE_ICONVCTL 1
#endif

char SOURCE_ENCODING[128];
char TARGET_ENCODING[128];

int DISABLE_TRANSLITERATION = 0;
int ENABLE_TRANSLITERATION = 0;
int ENABLE_ILSEQ_INVALID = 0;
int DISABLE_ILSEQ_INVALID = 0;

static int test_char(iconv_t cd, iconv_t cd_back, unsigned u,
                     char *input, size_t inputbytes,
                     char *output, size_t outbuflen, size_t *outputbytes,
                     int verbose, iconv_t cd_setup_back)
{
  size_t backbytes = 32;
  char input_back[backbytes];

  char *inbuf = input;
  size_t inbytesleft = inputbytes;
  char *outbuf = output;
  size_t outbytesleft = outbuflen;

  size_t res = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
  *outputbytes = outbuflen - outbytesleft;
  if (res == (size_t)-1) {
    if (errno == EILSEQ) {
      if (verbose) printf("EILSEQ U+%04x\n", u);
      return 1;  /* non-representable, returns EILSEQ */
    } else
      if (verbose) printf("ERROR U+%04x\n", u);
      return -1; /* test error */
  }
  if (!*outputbytes) {
    if (verbose) printf("DISCARDED U+%04x\n", u);
    return 1; /* non-representable, discarded */
  }
  
  inbuf = output;
  inbytesleft = *outputbytes;
  outbuf = input_back;
  outbytesleft = backbytes;
  res = iconv(cd_back, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
  
  if (res == (size_t)-1) {
    if (verbose) printf("BACK-ERROR U+%04x %s\n", u,
                        strerror(errno));
    return -1; /* test error */
  }
  
  size_t blen = backbytes - outbytesleft;
  
  if (!blen) {
    if (verbose) printf("BACK-DISCARDED-ERROR U+%04x\n", u);
    return -1; /* test error */
  }
    
  if (blen != inputbytes || memcmp(input, input_back, inputbytes)) {
    if (verbose) {
      printf("REPLACED U+%04x ->", u);
    
      /* for diagnostics, turn the replacement char(s) to UTF-32 */
      size_t buf32len = 32;
      char buf32[buf32len]; /* UTF32-BE */
      inbuf = input_back;
      inbytesleft = blen;
      outbuf = buf32;
      outbytesleft = buf32len;
      res = iconv(cd_setup_back, &inbuf, &inbytesleft, &outbuf,
                  &outbytesleft);
      unsigned char *s = (unsigned char *)buf32;
      while((unsigned char *)outbuf - s >= 4) {
        unsigned u1 = 0;
        u1 += (*s++) << 24;
        u1 += (*s++) << 16;
        u1 += (*s++) << 8;
        u1 += (*s++) << 0;
        printf(" U+%04x", u1);
      }
      printf(", %s ", SOURCE_ENCODING);
      for(int i = 0; i < inputbytes; i++)
        printf("%02x", ((unsigned char *)input)[i]);
      printf(" -> ");
      for(int i = 0; i < blen; i++)
        printf("%02x", ((unsigned char *)input_back)[i]);
      printf("\n");
    }
    return 2; /* non-representable, transliteration or substitution */
  }
  return 0; /* representable, ok */
}

#ifdef USE_ICONVCTL
static void report_iconvctl(iconv_t cd, int code, char *msg)
{
  int res = -2;
  if (!iconvctl(cd, code, &res))
    printf("iconvctl %s %d\n", msg, res);
  else
    printf("iconvctl %s failed: %s\n", msg, strerror(errno));
}

#endif

static int test_nonrepresentable()
{
    /* for conversion from UTF-32BE to the source encoding for the test */
    iconv_t cd_setup = iconv_open(SOURCE_ENCODING, "UTF-32BE");
    iconv_t cd_setup_back = iconv_open("UTF-32BE", SOURCE_ENCODING);
    
    /* for test conversion from source encoding to target */
    iconv_t cd = iconv_open(TARGET_ENCODING, SOURCE_ENCODING);
    iconv_t cd_back = iconv_open(SOURCE_ENCODING, TARGET_ENCODING);
    
    printf("source encoding: %s\n", SOURCE_ENCODING);
    printf("target encoding: %s\n", TARGET_ENCODING);

#ifdef USE_ICONVCTL
    int arg;

    if (ENABLE_TRANSLITERATION || DISABLE_TRANSLITERATION) {
      printf("transliteration: %s\n",
             ENABLE_TRANSLITERATION ? "enable" : "disable");
             
      report_iconvctl(cd, ICONV_GET_TRANSLITERATE, "ICONV_GET_TRANSLITERATE");

      arg = ENABLE_TRANSLITERATION ? 1 : 0;
      if (!iconvctl(cd, ICONV_SET_TRANSLITERATE, &arg)) {
        printf("iconvctl ICONV_SET_TRANSLITERATE %d succeeded.\n", arg);
      } else
        printf("iconvctl ICONV_SET_TRANSLITERATE %d failed: %s\n",
          arg, strerror(errno));
    }

    if (ENABLE_ILSEQ_INVALID || DISABLE_ILSEQ_INVALID) {
      printf("ilseq invalid: %s\n",
             ENABLE_ILSEQ_INVALID ? "enable" : "disable");
             
      report_iconvctl(cd, ICONV_GET_ILSEQ_INVALID, "ICONV_GET_ILSEQ_INVALID");

      arg = ENABLE_ILSEQ_INVALID ? 1 : 0;
      if (!iconvctl(cd, ICONV_SET_ILSEQ_INVALID, &arg)) {
        printf("iconvctl ICONV_SET_ILSEQ_INVALID %d succeeded.\n", arg);
      } else
        printf("iconvctl ICONV_SET_ILSEQ_INVALID %d failed: %s\n",
          arg, strerror(errno));
    }

    report_iconvctl(cd, ICONV_GET_TRANSLITERATE, "ICONV_GET_TRANSLITERATE");
    report_iconvctl(cd, ICONV_GET_DISCARD_ILSEQ, "ICONV_GET_DISCARD_ILSEQ");
    report_iconvctl(cd, ICONV_GET_ILSEQ_INVALID, "ICONV_GET_ILSEQ_INVALID");
#endif
    
    if (cd_setup == (iconv_t)-1 || cd_setup_back == (iconv_t)-1 || \
        cd == (iconv_t)-1 || cd_back == (iconv_t)-1)
        return -1;

    for(unsigned u = 0; u <= 0x10FFFF; u++) {

      char u32[4]; /* UTF-32BE */
      u32[0] = (u >> 24) & 0xff;
      u32[1] = (u >> 16) & 0xff;
      u32[2] = (u >> 8) & 0xff;
      u32[3] = (u >> 0) & 0xff;
      
      int buflen = 32;

      char source[buflen]; /* SOURCE encoding */
      size_t sourcebytes = 0;
      int res = test_char(cd_setup, cd_setup_back, u,
                     u32, 4, source, buflen, &sourcebytes,
                     0 /*verbose*/, NULL /*cd_setup_back*/);
      if (res)
        /* character not supported in source encoding */
        continue;
      
      char target[buflen]; /* TARGET encoding */
      size_t targetbytes = 0;
      res = test_char(cd, cd_back, u,
                 source, sourcebytes, target, buflen, &targetbytes,
                 1 /*verbose*/, cd_setup_back);
      if (!res)
        printf("OK U+%04x\n", u);
    }
    
    iconv_close(cd);
    iconv_close(cd_back);
    return 0;
}

int main(int argc, char **argv)
{
  if (argc >= 2)
    strcpy(SOURCE_ENCODING, argv[1]);
  else
    strcpy(SOURCE_ENCODING, "UTF-8");
    
  if (argc >= 3)
    strcpy(TARGET_ENCODING, argv[2]);
  else
    strcpy(TARGET_ENCODING, "CP1252");
    
#ifdef USE_ICONVCTL
  for(int i = 3; i < argc; i++) {
    if (!strcmp(argv[i], "--disable-transliteration"))
      DISABLE_TRANSLITERATION = 1;
    
    if (!strcmp(argv[i], "--enable-transliteration"))
      ENABLE_TRANSLITERATION = 1;

    if (!strcmp(argv[i], "--disable-ilseq-invalid"))
      DISABLE_ILSEQ_INVALID = 1;
    
    if (!strcmp(argv[i], "--enable-ilseq-invalid"))
      ENABLE_ILSEQ_INVALID = 1;
  }
#endif

  test_nonrepresentable();
  return 0;
}
