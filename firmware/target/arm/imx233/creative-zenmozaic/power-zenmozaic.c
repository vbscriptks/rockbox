/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2013 by Amaury Pouly
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "system.h"
#include "power.h"
#include "tuner.h"
#include "fmradio_i2c.h"
#include "pinctrl-imx233.h"
#include "power-imx233.h"

static bool tuner_enable = false;
static bool initialised = false;

static void init(void)
{
    /* CE is B2P15 (active high) */
    imx233_pinctrl_acquire(2, 15, "tuner power");
    imx233_pinctrl_set_function(2, 15, PINCTRL_FUNCTION_GPIO);
    imx233_pinctrl_enable_gpio(2, 15, true);
    initialised = true;
}

bool tuner_power(bool enable)
{
    if(!initialised)
        init();
    if(tuner_enable != enable)
    {
        imx233_pinctrl_set_gpio(2, 15, enable);
        sleep(HZ / 5);
        tuner_enable = enable;
    }
    return tuner_enable;
}

bool tuner_powered(void)
{
    return tuner_enable;
}