#define _POSIX_C_SOURCE 200809L

#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define CHIP_PATH "/dev/gpiochip0"
#define GPIO_OFFSET 17

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

	const struct timespec half_period = {
		.tv_sec = 0,
		.tv_nsec = 500000000L
	};
	
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
	
	//Configure GPIO 17 as an output
	if(gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT) < 0){
		perror("set direction");
		goto cleanup;
	}
	
	//Set initial output state to LOW
	if(gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE) < 0){
		perror("set_output_value");
		goto cleanup;
	}
	
	//Associate settings with GPIO offset 17
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

	gpiod_request_config_set_consumer(req_cfg, "pi-zero-led");

	//Request ownership of GPIO17
	request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

	if(!request){
		perror("gpiod_chip_request_lines");
		goto cleanup;
	}

	signal(SIGINT, handle_sigint);

	printf("Blinking GPIO17. Press Ctrl+C to stop\n");

	while(running){
	
		//LED On
		if(gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_ACTIVE) < 0){
			perror("set ACTIVE");
			goto cleanup;
		}

		if(nanosleep(&half_period, NULL) < 0 && errno != EINTR){
			perror("nanosleep");
			goto cleanup;
		}

		if(!running){
			break;
		}

		//LED Off
		if(gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_INACTIVE) < 0){
			perror("set INACTIVE");
			goto cleanup;
		}

		if(nanosleep(&half_period, NULL) < 0 && errno != EINTR){
			perror("nanosleep");
			goto cleanup;
		}
	}
	status = EXIT_SUCCESS;

cleanup:

//Turn off LED and release GPIO
if(request){
	if(gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_INACTIVE) < 0){
		perror("final LED Off");
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

//Compiled using: gcc -std=c11 -Wall -Wextra -Wpedantic blink.c -o blink $(pkg-config --cflags --libs libgpiod)
