// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wire.h
// COLLEGAMENTO degli strati: il solo posto (oltre a vos_main.cpp) che conosce tutti. Riempie i ganci del nucleo
// (core/vos_hooks.h) con le funzioni di pagina e package e iscrive le periferiche dei package. Prima cosa di setup().
#pragma once

void vosWire();
