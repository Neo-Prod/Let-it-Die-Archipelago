#ifndef LID_SHOP_TOWER_RECEIPT_PORT_H
#define LID_SHOP_TOWER_RECEIPT_PORT_H
#include "shop_tower_receipt_wire.h"
/* Immutable resident C-only provider. No engine types at the R26 boundary. */
typedef struct LidTowerReceiptPort {
    R15Context context;uint8_t catalogue[32];uint32_t ui,worker;void*host;
    int(*state)(void*,const uint8_t*);
    int(*peek)(void*,uint8_t*,uint64_t*);
    int(*published)(void*,uint64_t);
    int(*ack)(void*,const LidTowerPurchaseFrame*);
    int(*retire)(void*);void(*halt)(void*);
} LidTowerReceiptPort;
#endif
