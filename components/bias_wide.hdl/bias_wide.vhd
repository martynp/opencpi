-- THIS FILE WAS ORIGINALLY GENERATED ON Mon Jun 15 14:39:51 2015 EDT
-- BASED ON THE FILE: bias_widths.xml
-- YOU *ARE* EXPECTED TO EDIT IT
-- This file initially contains the architecture skeleton for worker: bias_widths

library IEEE; use IEEE.std_logic_1164.all; use ieee.numeric_std.all;
library ocpi; use ocpi.types.all; -- remove this to avoid all ocpi name collisions
architecture rtl of bias_wide_worker is
  signal doit : bool_t;
begin
-- Pure combinatorial implementation
  doit                <= ctl_in.is_operating and in_in.ready and out_in.ready;
-- WSI input interface outputs
  in_out.take         <= doit;
-- WSI output interface outputs
  out_out.give        <= doit;
  g0: for i in 0 to to_integer(nwide)-1 generate
    out_out.data(i*32+31 downto i*32) <=
      std_logic_vector(unsigned(in_in.data(i*32+31 downto i*32)) + props_in.biasValue);
  end generate g0;
  out_out.som         <= in_in.som;
  out_out.eom         <= in_in.eom;
  out_out.valid       <= in_in.valid;
  out_out.byte_enable <= in_in.byte_enable; -- only necessary due to BSV protocol sharing
  out_out.opcode      <= in_in.opcode;
end rtl;
