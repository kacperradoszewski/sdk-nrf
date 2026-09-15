# fmfu_smp_svr on the nRF9251 (internal)

Internal-only notes for testing the `fmfu_smp_svr` sample on the **nRF9251 DK**.
This is a work-in-progress nRF92 branch; the upstream `README.rst` (and its
`update_modem.py` flow) targets the nRF91 Series and does **not** apply here.

## Build and flash

Board target: `nrf9251dk/nrf9251/cpuapp`.

- Default UART baud rate (115200):

  ```
  west build -b nrf9251dk/nrf9251/cpuapp -p
  west flash
  ```

- 1 Mbaud (faster upload) — add the nRF92 UART overlay, which sets `uart133` to
  1000000:

  ```
  west build -b nrf9251dk/nrf9251/cpuapp -p -- -DEXTRA_DTC_OVERLAY_FILE=uart_nrf92.overlay
  west flash
  ```

## Upload the modem firmware

Do **not** use the included `update_modem.py` script (nRF91-only). Use
`nrfutil mcu-manager` instead. The `--mtu` value must match
`CONFIG_MCUMGR_TRANSPORT_NETBUF_SIZE` (512 in `prj.conf`).

- Example with default baud (115200):

  ```
  nrfutil mcu-manager serial modem-fw-upload \
      --firmware <mfw.zip> \
      --serial-port port=/dev/ttyACM0,baud_rate=115200 \
      --mtu 512
  ```

- Example with 1 Mbaud (when built with `uart_nrf92.overlay`):

  ```
  nrfutil mcu-manager serial modem-fw-upload \
      --firmware <mfw.zip> \
      --serial-port port=/dev/ttyACM0,baud_rate=1000000 \
      --mtu 512
  ```

After a successful upload, you must reboot the device to boot cellcore into the newly programmed
firmware.

## Note: `_signed` loader in the mfw .zip

The current release of `nrfutil mcu-manager` still expects an `ipc_dfu` loader
file with `_signed` in its name inside the modem firmware `.zip`. The nRF92 mfw
packages do not ship such a file, so some manual manipulation of the `.zip` is
required (rename the loader entry so its filename contains `_signed`) before
the upload will succeed.
