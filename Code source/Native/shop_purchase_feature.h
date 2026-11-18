#ifndef LID_SHOP_PURCHASE_FEATURE_H
#define LID_SHOP_PURCHASE_FEATURE_H
#ifndef LID_SHOP_PURCHASE_ENABLED
#define LID_SHOP_PURCHASE_ENABLED 0
#endif
#if LID_SHOP_PURCHASE_ENABLED
#if !LID_SHOP_CATALOGUE_ENABLED || !LID_MATERIAL_CAPABILITY_ENABLED || !LID_CHECKS_DELIVERY_ENABLED || !LID_CHECKS_DIAGNOSTIC_ENABLED
#error "Shop purchase requires verified catalogue/material/checks owner and worker"
#endif
#if !LID_SHOP_EXEC_SEH_ENABLED || !LID_MOMOKA_DISPATCH_SEH_ENABLED
#error "Shop purchase requires both compiler-finally execution paths"
#endif
#endif
#endif
