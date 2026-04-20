#pragma once

#include "py/gc.h"
#include "py/obj.h"
#include "py/runtime.h"
#include <stdio.h>

#include "supervisor/port.h"

#include <stdint.h>
#include <stdbool.h>

// #define DEBUG
void TR_Init(void);
void TR_Step(void);