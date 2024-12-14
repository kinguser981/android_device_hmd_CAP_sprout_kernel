/*
touchscreen_common_init.c

call all touchscreen init function here, for multi touchscreen compatable
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include "lct_tp_info.h"

#ifdef CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI
extern int32_t nvt_driver_init(void);
extern void nvt_driver_exit(void);
extern bool nt36xxx_reset_keep_high(void);
#endif

#ifdef CONFIG_TOUCHSCREEN_ILI9881X_HOSTDL_SPI
extern int ilitek_plat_dev_init(void);
extern void ilitek_plat_dev_exit(void);
extern bool ili9881x_reset_keep_high(void);
#endif

bool (*lcd_reset_keep_high)(void) = NULL;


extern char *saved_command_line;
static int __init touchscreen_common_init(void){
	if (strstr(saved_command_line, "androidboot.mode=charger") != NULL) {
		printk("androidboot.mode=charger, doesn't support touch in the charging mode!\n");
		return 0;
	}
	
//create longcheer procfs node
#ifdef CONFIG_TOUCHSCREEN_PROCFILE
	if (init_lct_tp_info("[Vendor]unkown,[FW]unkown,[IC]unkown\n", NULL) < 0) {
		printk("init_lct_tp_info Failed!\n");
	} else {
		printk("init_lct_tp_info Succeeded!\n");
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI
    if(nvt_driver_init() < 0){
		printk("init novatek touchscreen failed");
	}
	else{
		lcd_reset_keep_high = nt36xxx_reset_keep_high;
    	return 0;
	}
#endif
#ifdef CONFIG_TOUCHSCREEN_ILI9881X_HOSTDL_SPI
	if(ilitek_plat_dev_init() < 0){
		printk("init ilitek touchscreen failed");
	}
	else{
		lcd_reset_keep_high = ili9881x_reset_keep_high;
		return 0;
	}
#endif

	lcd_reset_keep_high = NULL;
    return 0;
}

static void __exit touchscreen_common_exit(void){
   nvt_driver_exit();
   ilitek_plat_dev_exit();
}

module_init(touchscreen_common_init);
module_exit(touchscreen_common_exit);

MODULE_AUTHOR("Longcheer touchscreen module");
MODULE_DESCRIPTION("Longcheer touchscreen common init");
MODULE_LICENSE("GPL v2");
