# smbus_nct7802
nct7802  utilities for QNX 6.5
Утилита для просмотра показаний напряжения и температуры датчика nct7802 установленном на SMBus шине по адресу 0х2С 
на плате Kontron COMe-bHL6.

Перед запуском убедиться что запущен ресурс менджер SMBus шины https://github.com/llmike/smbus/tree/master/smb-ich.
Так же будет работать на чипсете PCH C220, для этого изменить device_id на соответствующий адрес.
