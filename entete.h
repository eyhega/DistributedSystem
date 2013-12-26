#include <rpc/types.h>
#include <rpc/xdr.h>
#include <string.h>
#define ARITH_PROG 0x12123123
#define ARITH_VERS1 1
#define REGISTER_PROC 1
#define BUFF_SIZE 50

struct couple
{
  char _name[BUFF_SIZE];
  unsigned short _port;
};
typedef struct couple couple;

int xdr_couple(XDR *xdrp, struct couple *p);
char *registerClient();
