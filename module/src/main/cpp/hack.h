//
// Created by Perfare on 2020/7/4.
//

#ifndef ZYGISK_IL2CPPDUMPER_HACK_H
#define ZYGISK_IL2CPPDUMPER_HACK_H

#include <stddef.h>
#include "zygisk.hpp"

void hack_prepare(zygisk::Api *api, const char *game_data_dir, void *data, size_t length);

#endif //ZYGISK_IL2CPPDUMPER_HACK_H
