clevo_backlit
=============

Clevo Keyboard Backlit Driver 

Originally Clevo_wmi driver has to be compile everytime when we install new kernel but this configuration has added DKMS support for this driver We don't have re-compile all the time.

When we install the driver and enable it. We should see "/sys/devices/platform/clevo_wmi/kbled" this directory and in that directory we should able change backlit color,give effect and all other stuff we can do it. 

## Example Usage


	cd /sys/devices/platform/clevo_wmi/kbled
	su -c 'echo 111 > middle'   // For white color in middle part of the keyboard.

## Color palatte codes

|  RGB Color	| Color List	|
| ------------- | ------------- |
|000  		|Off		|
|111  		|White		|
|100  		|Green		|
|010  		|Red		|
|001  		|Blue		|
|110  		|Yellow		|
|011  		|Purple		|
|101  		|Aqua		|

## Other Keyboard Combinations
|--------------| 
|all_off       |     
|fade  	       |
|left_right    |  
|out_mid       |
|random        |
|random_flicker|  
|right 	       |
|brightness    |  
|left  	       |
|middle        | 
|pattern_off   |  
|random_fade   |  
|raw           |     
|single_fade   |



