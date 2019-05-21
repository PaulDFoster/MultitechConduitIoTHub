
#ifndef DEVICE_PROVISIONING_H
#define DEVICE_PROVISIONING_H

#include "azure_prov_client/prov_device_ll_client.h"

void Provisioning();
void register_device_callback(PROV_DEVICE_RESULT register_result, const char* iothub_uri, const char* device_id, void* user_context);
void registation_status_callback(PROV_DEVICE_REG_STATUS reg_status, void* user_context);

#endif /*DEVICE_PROVISIONING_H*/