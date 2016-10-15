clevo_backlit
=============

Clevo Keyboard Backlit Driver 

Originally Clevo_wmi driver has to be compiled everytime when we install a new kernel but this configuration has added DKMS support for this driver We don't have re-compiled all the time.

When we installed the driver and enable it, we should see "/sys/devices/platform/clevo_wmi/kbled" this directory and in that directory we should able to change backlit color, predefined color effects.  

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


Keyboard Options;  We could do to change keyboard light effects or only change part of keyboard(left,middile,right) or any custom you can define option would you like to have it. 

|Keyboard Options|
| ------------ | 
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



