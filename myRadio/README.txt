# myradio refactor (phase 1)
This sketch is refactored to keep the main .ino small.

Files:
- myradio_refactor.ino: only setup()/loop() wrappers.
- app_impl.ino: your original code, with setup()/loop() renamed to app_setup()/app_loop().

If you already use conf/display_profile*.h, they are copied into conf/.
Keep your existing Lovyan_config.h as-is.

How to use:
1) Copy the whole folder contents into your Arduino sketch folder (or open myradio_refactor.ino inside this folder).
2) Ensure app_impl.ino is in the same sketch.
3) Compile/upload as usual.
