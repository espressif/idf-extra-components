-- SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
-- SPDX-License-Identifier: Apache-2.0
--
-- fibonacci.lua
-- This script calculates the 10th number in the Fibonacci sequence and prints it.

local function fibonacci(n)
    if n <= 1 then return n end
    return fibonacci(n - 1) + fibonacci(n - 2)
end

local fib_10 = fibonacci(10)
print('Fibonacci of 10 is: ' .. fib_10)
return fib_10
