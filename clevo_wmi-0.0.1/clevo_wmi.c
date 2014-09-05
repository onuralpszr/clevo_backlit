/*
 * clevo_wmi.c: Support for Clevo laptop (P1x0EM and others) backlit keyboard/WMI
 * Copyright 2013 Steven David Seeger steven.seeger@flightsystems.net
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

/* NOTE: there is no spec for any of this. it is purely guesswork! */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/dmi.h>
#include <linux/platform_device.h>
#include <linux/pm.h>

MODULE_DESCRIPTION("Support for Clevo laptop (P1x0EM and others) backlit keyboard/WMI");
MODULE_AUTHOR("Copyright 2013 Steven David Seeger steven.seeger@flightsystems.net");
MODULE_LICENSE("GPL v2");

/* It is possible that this file will want to be branched out for other laptops like ASUS. This is a starting point. */

#define CLWMI_GET_GUID "ABBC0F6D-8EA1-11d1-00A0-C90629100000"

static struct platform_driver platform_driver;
static struct platform_device *platform_device;

struct method {
    const char *guid;
    u8 instance;
    u32 methodid;
};

/* Some methods on the GUID take 16-bit arguments, so this is here for reference later */
#if 0
struct method_args16 {
    struct method m;
    u16 data;
};
#endif

/* The clevo stuff was taken from the clevo-extra project */
struct method_args32 {
    struct method m;
    u32 data;
};

static uint32_t kbled_val = 0x0000a111; //holds the last written kbled value. default is all blue on my machine

static acpi_status clevo_wmi_exec_method32(struct method_args32 *in, u32 *out)
{
    acpi_status status;
    u32 outbuf = in->data;
    struct acpi_buffer input = {sizeof(u32), &outbuf};
    struct acpi_buffer output = {ACPI_ALLOCATE_BUFFER, NULL};
    union acpi_object *obj;

    status = wmi_evaluate_method(in->m.guid, in->m.instance, in->m.methodid, &input, &output);
    if(ACPI_FAILURE(status)) return status;

    obj = (union acpi_object *)output.pointer;

    if(out && obj && (obj->type == ACPI_TYPE_INTEGER)) *out = (u32)obj->integer.value;

    kfree(output.pointer);
    return AE_OK;
}

static acpi_status clevo_wmi_kbled_exec(uint32_t val)
{
    struct method_args32 args;

    args.m.guid = CLWMI_GET_GUID;
    args.m.instance = 1;
    args.m.methodid = 103;
    args.data = val;

    return clevo_wmi_exec_method32(&args, NULL);
}

struct color_attribute {
    struct device_attribute attr;
    uint32_t shift;
};

/* for color use binary input ABCD where all are 0 or 1.
 * A is don't know/don't care
 * B is green on/off
 * C is red on/off
 * D is blue on/off
 */
static ssize_t kbled_show_color(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct color_attribute *ca = (struct color_attribute*)attr;
    int index;
    
    buf[4] = '\n';
    buf[5] = 0;

    for(index=3; index>=0; --index) {
        buf[3-index] = ((kbled_val>>ca->shift>>index)&1)+'0';
    }

    return 5;
}

static ssize_t kbled_store_color(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct color_attribute *ca = (struct color_attribute*)attr;
    
    int scratch=0;
    int index;
    
    if((count<4)||(count>6)) return -EINVAL;
    
    kbled_val &= ~(0xf<<ca->shift); //clear out old color
    for(index=(count-2); index>=0; --index) {
        if((buf[index]!='0')&&(buf[index]!='1')) return -EINVAL;
        scratch|=((buf[index]-'0')<<((count-2)-index));
    }
    kbled_val |= scratch<<ca->shift; //put in newvalue
    
    if(clevo_wmi_kbled_exec(kbled_val) != AE_OK)
        return -ENODEV;
    
    return count;
}

#define KBLED_ATTR_COLOR(name, shift) \
    static struct color_attribute kbled_color_##name = \
    { __ATTR(name, 0664, kbled_show_color, kbled_store_color), shift }

KBLED_ATTR_COLOR(left, 0);
KBLED_ATTR_COLOR(middle, 4);
KBLED_ATTR_COLOR(right, 8);

/* for brightness, you can use 0-10. 10 enables the fn keys to adjust */
static ssize_t kbled_show_brightness(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", ((kbled_val&0xf000)>>12));
}

static ssize_t kbled_store_brightness(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int val;
    
    if(sscanf(buf, "%d", &val)!=1) return -EINVAL;
    if((val<0)||(val>10)) return -EINVAL;
    
    kbled_val &= ~0xf000;
    kbled_val |= (val<<12);
    
    if(clevo_wmi_kbled_exec(kbled_val)!=AE_OK)
        return -ENODEV;
    
    return count;
}

static struct device_attribute brightness = __ATTR(brightness, 0664, kbled_show_brightness, kbled_store_brightness);

static ssize_t kbled_pattern_off(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    if(buf[0]!='1') return -EINVAL; //only send this a 1
    
    kbled_val &= 0xffff; //chop off the pattern stuff
    if((clevo_wmi_kbled_exec(kbled_val|0x10000000)!=AE_OK)||(clevo_wmi_kbled_exec(kbled_val)))
        return -ENODEV;
    
    return count;
}

static struct device_attribute pattern_off = __ATTR(pattern_off, 0664, NULL, kbled_pattern_off);

static ssize_t kbled_all_off(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    if(buf[0]!='1') return -EINVAL;
    
    kbled_val = 0x20000000;
    if(clevo_wmi_kbled_exec(kbled_val)!=AE_OK) return -ENODEV;
    
    return count;
}

static struct device_attribute all_off = __ATTR(all_off, 0664, NULL, kbled_all_off);

static ssize_t kbled_show_raw(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "0x%x\n", kbled_val);
}

/* write in hex */
static ssize_t kbled_store_raw(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{    
    if((sscanf(buf, "%x", &kbled_val))!=1) return -EINVAL;
    
    if(clevo_wmi_kbled_exec(kbled_val)!=AE_OK)
        return -ENODEV;
    
    return count;
}

static struct device_attribute raw = __ATTR(raw, 0664, kbled_show_raw, kbled_store_raw);

static ssize_t kbled_show_fade(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "0x%x\n", ((kbled_val&0xf0000000)==0x10000000) ? ((kbled_val&0x30000)>>16) : 0);
}

//accepts a speed value 0-3. 0 is off, 1 slow, 2 faster, 3 fastest
static ssize_t kbled_store_fade(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    uint32_t val;
    
    if((sscanf(buf, "%d", &val))!=1) return -EINVAL;
    if(val>3) return -EINVAL;
    
    kbled_val &= ~0xf0000;
    kbled_val |= (val<<16);
    kbled_val |= 0x10000000;
    
    if(clevo_wmi_kbled_exec(kbled_val)!=AE_OK)
        return -ENODEV;
    
    return count;
}

static struct device_attribute fade = __ATTR(fade, 0664, kbled_show_fade, kbled_store_fade);

static ssize_t kbled_show_random_fade(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "0x%x\n", ((kbled_val&0x30000000)==0x30000000) ? (((kbled_val&0x3000000)>>16) | ((kbled_val&0xff0000)>>16)) : 0);
}

/* accepts a hex number XYY where X is 0-3 (0 is off)
 * and YY is anything. X is speed (1 slow, 2 faster, 3 fastest)
 * YY is the number of times to repeat the current color before changing */
static ssize_t kbled_store_random_fade(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    uint32_t val;
    
    if((sscanf(buf, "%x", &val))!=1) return -EINVAL;
    if(val>0x3ff) return -EINVAL;
    
    kbled_val &= ~0xff000000;
    kbled_val |= ((val&0x300)<<16);
    kbled_val |= ((val&0xff)<<16);
    kbled_val |= 0x30000000;
    
    if(clevo_wmi_kbled_exec(kbled_val)!=AE_OK)
        return -ENODEV;
    
    return count;
}

static struct device_attribute random_fade = __ATTR(random_fade, 0664, kbled_show_random_fade, kbled_store_random_fade);


struct pattern_attribute {
    struct device_attribute attr;
    uint8_t pattern;
};

static ssize_t kbled_show_pattern(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct pattern_attribute *pa = (struct pattern_attribute*)attr;
    
    buf[0] = '0'+(((kbled_val&0xf0000000)>>28) == pa->pattern);
    buf[1] = '\n';
    buf[2] = 0;
    
    return 2;
}

static ssize_t kbled_store_pattern(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct pattern_attribute *pa = (struct pattern_attribute*)attr;

    if(buf[0]!='1') return -EINVAL;

    kbled_val &= 0xffff; //chop off pattern stuff
    kbled_val |= (pa->pattern<<28);
    
    if(clevo_wmi_kbled_exec(kbled_val) != AE_OK)
        return -ENODEV;
    
    return count;
}

#define KBLED_ATTR_PATTERN(name, pattern) \
    static struct pattern_attribute kbled_pattern_##name = \
    { __ATTR(name, 0664, kbled_show_pattern, kbled_store_pattern), pattern }

KBLED_ATTR_PATTERN(random, 0x7);
KBLED_ATTR_PATTERN(left_right, 0x8);
KBLED_ATTR_PATTERN(out_mid, 0x9);
KBLED_ATTR_PATTERN(random_flicker, 0xa);
KBLED_ATTR_PATTERN(single_fade, 0xb);
    
static struct attribute *kbled_attrs[] = {
    &kbled_color_left.attr.attr,
    &kbled_color_middle.attr.attr,
    &kbled_color_right.attr.attr,
    &brightness.attr,
    &pattern_off.attr,
    &all_off.attr,
    &raw.attr,
    &fade.attr,
    &random_fade.attr,
    &kbled_pattern_random.attr.attr,
    &kbled_pattern_left_right.attr.attr,
    &kbled_pattern_out_mid.attr.attr,
    &kbled_pattern_random_flicker.attr.attr,
    &kbled_pattern_single_fade.attr.attr,
    NULL
};

static const struct attribute_group platform_attribute_group = {
    .name = "kbled",
    .attrs = kbled_attrs
};

static int clevo_wmi_remove(struct platform_device *device)
{
    /* any driver unregister stuff here */
    sysfs_remove_group(&platform_device->dev.kobj, &platform_attribute_group);
    return 0;
}

static int __init clevo_wmi_probe(struct platform_device *pdev)
{
    if(!wmi_has_guid(CLWMI_GET_GUID)) {
        pr_err("Clevo Get GUID not found\n");
        return -ENODEV;
    }
    
    return sysfs_create_group(&pdev->dev.kobj, &platform_attribute_group);
}

static int clevo_wmi_resume(struct device *dev)
{
    if(clevo_wmi_kbled_exec(kbled_val))
        return -ENODEV;
    
    return 0;
}

static const struct dev_pm_ops clevo_wmi_pm_ops = {
    .resume = clevo_wmi_resume
};

static int __init clevo_wmi_init(void)
{
    platform_driver.remove = clevo_wmi_remove;
    platform_driver.driver.owner = THIS_MODULE;
    platform_driver.driver.name = "clevo_wmi";
    platform_driver.driver.pm = &clevo_wmi_pm_ops;
    
    platform_device = platform_create_bundle(&platform_driver, clevo_wmi_probe, NULL, 0, NULL, 0);
    if(IS_ERR(platform_device))
        return PTR_ERR(platform_device);
 
    if((clevo_wmi_kbled_exec(0x20000000)!=AE_OK)||(clevo_wmi_kbled_exec(kbled_val)!=AE_OK)) {
        platform_device_unregister(platform_device);
        platform_driver_unregister(&platform_driver);

        pr_err("could not communication with clevo WMI\n");
        return -ENODEV;
    }
    
    pr_info("Clevo WMI driver loaded\n");
    return 0;
}

static void __exit clevo_wmi_exit(void)
{
    platform_device_unregister(platform_device);
    platform_driver_unregister(&platform_driver);
    
    pr_info("Clevo WMI driver unloaded\n");
}

#if 0
static int __init wmi_poke_init(void)
{
    struct method_args32 args;

    u32 ret=0;
    acpi_status status;

    args.m.guid = CLWMI_GET_GUID;
    args.m.instance = 1;
    args.m.methodid = 103;
    args.data = 0x7000a124;

    if(!wmi_has_guid(CLWMI_GET_GUID)) {
        printk("no clevo WMI get guid\n");
        return -ENODEV;
    }

    if((status=wmi_exec_method32(&args, &ret))!=AE_OK) {
        printk("error in wmi_exec_method %d\n", status);
        return -EINVAL;
    }

    printk("success! status was %d ret was 0x%x (%d)\n", status, ret, ret);
    return 0;
}
#endif

module_init(clevo_wmi_init);
module_exit(clevo_wmi_exit);

