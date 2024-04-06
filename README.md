# ESPHome custom component ARG IR Remote Climate

Tested with ARG CSH-07OB and remote control KKG26A-C1

Support receiver

General information on the climate component: https://esphome.io/components/climate/climate_ir.html

Example
```yaml

external_components:
- source:
    type: git
    url: https://github.com/Velesey/esphome_arg_ir
    ref: main
  components: [arg_ir]

***

climate:
  - platform: arg_ir
    name: "AC"
    sensor: room_temperature
```
