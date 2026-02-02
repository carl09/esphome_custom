"""Daikin 312-bit protocol climate component."""

import esphome.codegen as cg

CODEOWNERS = ["@carl09"]

CONF_DAIKIN_312_ID = "daikin_312_id"

daikin_312_ns = cg.esphome_ns.namespace("daikin_312")
Daikin312Climate = daikin_312_ns.class_("Daikin312Climate")
