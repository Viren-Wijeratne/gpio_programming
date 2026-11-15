//Detect a button attached to GPIO 23
#define _POSIX_C_SOURCE 200809L

#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define CHIP_PATH "/dev/gpiochip0"
#define GPIO_OFFSET 23

static volatile sig_atomic_t running = 1;

static void handle_sigint(int signal_number){
	(void)signal_number;
	running = 0;
}

int main(void){

	struct gpiod_chip *chip = NULL;
	struct gpiod_line_settings *settings = NULL;
	struct gpiod_line_config *line_cfg = NULL;
	struct gpiod_request_config *req_cfg = NULL;
	struct gpiod_line_request *request = NULL;

	const unsigned int offset = GPIO_OFFSET;

	/*const struct timespec half_period = {
		.tv_sec = 0,
		.tv_nsec = 500000000L
	};*/
	
	int status = EXIT_FAILURE;

	//Open the GPIO chip
	chip = gpiod_chip_open(CHIP_PATH);

	if(!chip){
		perror("gpiod_chip_open");
		goto cleanup;
	}
	
	//Create line settings
	settings = gpiod_line_settings_new();

	if(!settings){
		perror("gpiod_line_settings_new");
		goto cleanup;
	}
	
	//Configure GPIO 23 as an output	       GPIOD_LINE_DIRECTION_OUTPUT is also an option
	if(gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT) < 0){
		perror("set direction");
		goto cleanup;
	}
	
	//Set initial output state to HIGH
	if(gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_ACTIVE) < 0){
		perror("set_output_value");
		goto cleanup;
	}
	
	//Associate settings with GPIO offset 23
	line_cfg = gpiod_line_config_new();

	if(!line_cfg){
		perror("gpiod_line_config_new");
		goto cleanup;
	}

	if(gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings) < 0){
		perror("add_line_settings");
		goto cleanup;
	}

	//Give the request a descriptive name
	req_cfg = gpiod_request_config_new();

	if(!req_cfg){
		perror("gpiod_request_config_new");
		goto cleanup;
	}

	gpiod_request_config_set_consumer(req_cfg, "pi-toggle-button");

	//Request ownership of GPIO23 from the kernel, if another process owns the requested line, this request fails 
	request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

	if(!request){
		perror("gpiod_chip_request_lines");
		goto cleanup;
	}

	signal(SIGINT, handle_sigint);

	printf("GPIO23 will be detecting a button press. Press Ctrl+C to stop\n");

	int toggle = 0;

	while(running){
	
		//Switch Pressed					Turning the GPIO pin LOW(3.3.v)
		int pressed;
		if(pressed = gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_INACTIVE) < 0){
			perror("set INACTIVE");
			goto cleanup;
		}
		if(pressed == 0 && toggle == 0){
			toggle = 1;
			printf("The button was pressed\n");
		}

		if(!running){
			break;
		}

		//Switch not Pressed
		int notpressed;					  //Turning the GPIO pin HIGH
		if(notpressed = gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_ACTIVE) < 0){
			perror("set ACTIVE");
			goto cleanup;
		}
		if(notpressed == 0){
			toggle = 0;
			printf("The button is not pressed\n");
		}

	}
	status = EXIT_SUCCESS;

cleanup:

//Release GPIO
if(request){
	if(gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_ACTIVE) < 0){
		perror("final Switch ON");
		status = EXIT_FAILURE;
	}
	gpiod_line_request_release(request);
}	

//Free allocated resources
if(req_cfg){
	gpiod_request_config_free(req_cfg);
}

if(line_cfg){
	gpiod_line_config_free(line_cfg);
}

if(settings){
	gpiod_line_settings_free(settings);
}

if(chip){
	gpiod_chip_close(chip);
}



return status;
}

/*
- Compiled using: gcc -std=c11 -Wall -Wextra -Wpedantic blink.c -o blink $(pkg-config --cflags --libs libgpiod)

Controlling the GPIO involves several steps:
+--------------------+----------------------------------------------------------+
|       Object       |				Purpose				|
+--------------------+----------------------------------------------------------+
|gpiod_chip          |Represent a GPIO controller				|
|gpiod_line_settings |Describes direction, bias, edge detection and output value|
|gpiod_line_config   |Associates settings with specific GPIO offsets   		|
|gpiod_request_config|Describes the request, including its consumer name	|
|gpiod_line_request  |Represents ownership of the requested GPIO lines		|
+--------------------+----------------------------------------------------------+
- We should release the used resources/objects before terminating the program 
- Identify the available GPIO chips: gpiodetect
- Inspect the GPIO lines: gpioinfo

- It should be stated the limitation of the gpiod library. Capabilities include,
	- Digital GPIO Input
	- Digital GPIO Output
	- Button detection and interrupts

*/
