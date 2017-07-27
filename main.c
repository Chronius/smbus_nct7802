/*
 * This utility finds the sensor nct7802 on COMe-bHL6 through the SMBus controller
 * and displays the temperature and voltage readings
 *
 * Copyright (C) 2017  Bulat Abuzarov
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <dcmd_smbus.h>
#include <errno.h>
#include <unistd.h>

#define REG_BANK		0x00
#define REG_TEMP_LSB		0x05
#define REG_TEMP_PECI_LSB	0x08
#define REG_VOLTAGE_LOW		0x0f
#define REG_FANCOUNT_LOW	0x13
#define REG_START		0x21
#define REG_MODE		0x22
#define REG_PECI_ENABLE		0x23
#define REG_FAN_ENABLE		0x24
#define REG_VMON_ENABLE		0x25
#define REG_VENDOR_ID		0xfd
#define REG_CHIP_ID		0xfe
#define REG_VERSION_ID		0xff

static const uint8_t REG_TEMP[6] = { 0x01, 0x02, 0x03, 0x04, 0x06, 0x07 };
static char *NameRegTemp[] = {
		"Remote Diode 1 PCH",
		"Remote Diode 2",
		"Remote Diode 3",
		"Local Diode SYS",
		"PECI 0 CPU	",
		"PECI 1",
		"PECI 2" };

static char *NameRegVolt[] = {
		"VCore 3V3",
		"VCC  VBAT",
		"VSEN1",
		"VSEN2",
		"VSEN3 12V" };


static const uint8_t REG_VOLTAGE[5] = { 0x09, 0x0a, 0x0c, 0x0d, 0x0e };
//  look this man to understand where these numbers come from 
//  https://linux.die.net/man/5/sensors.conf
static const float VOLTAGE_MULT[5] = {1, 1.698947368, 0, 0, 7.17};

int fd;
smbus_cmd_data_t cmd;

uint16_t i2c_smbus_read_byte_data(int device, uint8_t addr) {

	cmd.cmd_type = SMBUS_CMD_BYTE_DATA;
	cmd.slv_addr = device;
	cmd.smbus_cmd = addr;
	cmd.length = 0x00;
	int res;
	res = devctl(fd, DCMD_SMBUS_READ, &cmd, sizeof(cmd), NULL);
	if (res == EOK)
		return cmd.data;
	else
		return 0;
}

uint16_t i2c_smbus_write_byte_data(int device, uint8_t addr, uint16_t data) {
	int res;

	cmd.data = data;
	cmd.cmd_type = SMBUS_CMD_BYTE_DATA;
	cmd.slv_addr = device;
	cmd.smbus_cmd = addr;
	cmd.length = 0x00;
	res = devctl(fd, DCMD_SMBUS_WRITE, &cmd, sizeof(cmd), NULL);
	if (res == EOK)
		return cmd.data;
	else
		return 0;
}

void show_temp(int device) {
	unsigned int t1 = 0, t2 = 0;
	int i;
	float res;
	int reg_temp_low = 0;

	printf("\n*******TEMPERATURE********\n");
	for (i = 0; i < sizeof(REG_TEMP); i++) {
		t1 = i2c_smbus_read_byte_data(device, REG_TEMP[i]);
		if (t1 <= 0)
			continue;
		if (reg_temp_low) {
			t2 = i2c_smbus_read_byte_data(device, REG_TEMP_LSB);
			if (t2 <= 0)
				continue;
			t2 = ((t2 & 0xe0) >> 5) * 125;
		}
		res = (t1 << 8) / 32 * 125 + t2;
		printf("%s \tVal = %g\n", NameRegTemp[i], res / 1000);
		delay(100);
	}
	printf("**************************\n");
}

void show_voltage(int device) {
	printf("\n****VOLTAGE******\n");
	int index = 0;
	uint16_t voltage = 0;
	float compute_val;

	printf("Name\t\tCompute val\tRaw value\n");
	for (index = 0; index < sizeof(REG_VOLTAGE); index++) {
		voltage = 0;
		voltage = i2c_smbus_read_byte_data(device, REG_VOLTAGE[index]) << 2;
		voltage |= i2c_smbus_read_byte_data(device, REG_VOLTAGE_LOW) >> 6;
		if (index == 0)
				voltage = voltage * 4;
			else {
				voltage = voltage * 2; ;

			}
		compute_val = ((float) voltage) / 1000  * VOLTAGE_MULT[index];
		if (compute_val == 0)
			continue;
		printf("%s\t%g\t\t%d\n", NameRegVolt[index], compute_val, voltage);
		delay(100);
	}

	printf("*****************\n");
	return;
}

int nct7802_detect(int device)
{
	int reg;

	/*
	 * Chip identification registers are only available in bank 0,
	 * so only attempt chip detection if bank 0 is selected
	 */
	reg = i2c_smbus_read_byte_data(device, REG_BANK);
	if (reg != 0x00)
		return -ENODEV;

	reg = i2c_smbus_read_byte_data(device, REG_VENDOR_ID);
	if (reg != 0x50)
		return -ENODEV;

	reg = i2c_smbus_read_byte_data(device, REG_CHIP_ID);
	if (reg != 0xc3)
		return -ENODEV;

	reg = i2c_smbus_read_byte_data(device, REG_VERSION_ID);
	if (reg < 0 || (reg & 0xf0) != 0x20)
		return -ENODEV;

	/* Also validate lower bits of voltage and temperature registers */
	reg = i2c_smbus_read_byte_data(device, REG_TEMP_LSB);
	if (reg < 0 || (reg & 0x1f))
		return -ENODEV;

	reg = i2c_smbus_read_byte_data(device, REG_TEMP_PECI_LSB);
	if (reg < 0 || (reg & 0x3f))
		return -ENODEV;

	reg = i2c_smbus_read_byte_data(device, REG_VOLTAGE_LOW);
	if (reg < 0 || (reg & 0x3f))
		return -ENODEV;

	printf("\nDetect chip sensor nct7805 \n");

	return EOK;
}

int main(int argc, char *argv[]) {
	int status;
	int smbus_id;
	int device_id;
	int found_smbus = 0;

	for (smbus_id = 0; smbus_id <= 99; smbus_id++) {
		char smbus_name[PATH_MAX];

		snprintf(smbus_name, PATH_MAX, "/dev/smb%d", smbus_id);

		fd = open(smbus_name, O_RDWR);
		if (fd < 0) {
			if (smbus_id == 99) {
				if (!found_smbus) {
					fprintf(stderr, "Can't find any SMBus device!\n");
				}
				exit(1);
			}
			continue;
		} else {
			found_smbus = 1;
		}
		fprintf(stdout, "Found SMBUS%d\n", smbus_id);

		/* Check if device is available  */
		device_id = 0x2C;	// This address is specified in datasheet on Kontron COMe-bHL6, page 64

		cmd.cmd_type = SMBUS_CMD_BYTE_DATA;
		cmd.slv_addr = device_id;
		cmd.smbus_cmd = 0x00;
		cmd.length = 0x00;
		status = devctl(fd, DCMD_SMBUS_READ, &cmd, sizeof(cmd), NULL);
		if (status == EOK)
			fprintf(stdout, "    Found device presence at address 0x%02X\n", device_id);

		/*Read byte on address*/
		fprintf(stdout, "REG_CHIP_ID = 0x%02X ", i2c_smbus_read_byte_data(
				device_id, REG_CHIP_ID));
		fprintf(stdout, "Version = 0x%02X ", i2c_smbus_read_byte_data(
						device_id, REG_VERSION_ID));

		nct7802_detect(device_id);

		i2c_smbus_write_byte_data(device_id, REG_BANK, 0);
		i2c_smbus_write_byte_data(device_id, REG_START, 0x01);

		i2c_smbus_write_byte_data(device_id, REG_MODE, 0x3F);
		/* Enable Vcore and VCC voltage monitoring */
		i2c_smbus_write_byte_data(device_id, REG_VMON_ENABLE, 0x3);
		/*Read Volt and show*/
		show_voltage(device_id);

		/* Enable local temperature sensor */
		i2c_smbus_write_byte_data(device_id, REG_MODE, 0x40);
		/*Read Temp and show*/
		show_temp(device_id);

	}


	return EXIT_SUCCESS;
}

