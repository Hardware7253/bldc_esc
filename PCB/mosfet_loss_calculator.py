from prefixed import Float
import math

ids = 20 # Switching current
vds_max = 12.6 # Voltage we are switching
ig = 1.04  # Gate drive current (during MOSFET switch on/off)

# MOSFET parameters
rds_on = 1e-3
q_loss = 23e-9 # Charge associated with the loss region (Qgd + Q{Vth < V < Vmiller})
q_gate = 103e-9 # Charge to reach max vgs

# Application parameters
duty = 1
f = 100e3

p_conduction_max = ids**2 * rds_on
p_conduction_av = p_conduction_max * duty

# Rise time during the loss period (after Vth reached until the end of the miller plateau)
t_rise_loss = q_loss / ig 
t_fall_loss = t_rise_loss

p_sw = 0.5 * ids * vds_max * (t_rise_loss + t_fall_loss) * f

print("Losses")
print(f"P conduction (max): {Float(p_conduction_max):.3h}W")
print(f"P conduction (average): {Float(p_conduction_av):.3h}W")
print(f"P switch: {Float(p_sw):.3h}W")
print(f"P total: {Float(p_conduction_av + p_sw):.3h}W")
print(f"Loss time (per cycle): {Float(t_rise_loss + t_fall_loss):.3h}S")
print("")

t_rise = q_gate / ig
t_fall = t_rise
ig_av = ig * ((t_rise + t_fall) * f)
ig_rms = math.sqrt(f * (ig**2) * (t_rise + t_fall))
print(f"Rise time: {Float(t_rise):.3h}S")
print(f"Average gate drive current: {Float(ig_av):.3h}A")
print(f"RMS gate drive current: {Float(ig_rms):.3h}A")
