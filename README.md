# ARG IR climate component for ESPHome

External ESPHome component for ARG air conditioners using the KKG26A-C1 IR
protocol. Tested with ARG CSH-07OB.

Both IR transmission and reception are supported.

## Example

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Velesey/esphome_arg_ir
      ref: main
    components: [arg_ir]

remote_transmitter:
  id: ir_transmitter
  pin: GPIO4
  carrier_duty_percent: 50%

# Optional: enables synchronization from the original remote.
remote_receiver:
  id: ir_receiver
  pin:
    number: GPIO5
    inverted: true
  dump: raw

climate:
  - platform: arg_ir
    name: AC
    transmitter_id: ir_transmitter
    receiver_id: ir_receiver
```

The component targets the current ESPHome `climate_ir` API. See the
[ESPHome climate IR documentation](https://esphome.io/components/climate/climate_ir/)
for shared options such as `sensor`, `supports_cool`, and `supports_heat`.
