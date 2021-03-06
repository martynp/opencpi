-- THIS FILE WAS ORIGINALLY GENERATED ON Tue Jun  2 08:35:09 2015 EDT
-- BASED ON THE FILE: lime_tx.xml
-- YOU *ARE* EXPECTED TO EDIT IT
-- This file initially contains the architecture skeleton for worker: lime_tx

library IEEE; use IEEE.std_logic_1164.all; use ieee.numeric_std.all;
library ocpi; use ocpi.types.all; -- remove this to avoid all ocpi name collisions
architecture rtl of lime_tx_worker is
begin
  -- Route the raw property signals to the lime_spi
  rawprops_out.present     <= '1';
  rawprops_out.reset       <= ctl_in.reset;
  rawprops_out.raw         <= props_in.raw;
  props_out.raw            <= rawprops_in.raw;
  props_out.other_present  <= rawprops_in.present(1);
  -- FIXME: should this be "not ctl_in.reset and ctl_in.is_operating"?
  txen <= '1';
end rtl;
