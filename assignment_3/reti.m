close all;
clear;
clc;

rtt_test = readmatrix("rtt_test.txt");
thput_test = readmatrix("thput_test.txt");

plots = figure('units', 'normalized', 'outerposition', [0 0 1 1]);
axis tight;
grid on;

subplot(1, 2, 1);
x = rtt_test(:, 1);
y = rtt_test(:, 3);
c = polyfit(x, y, 1);
xFit = linspace(min(x), max(x), 100);
yFit = polyval(c, xFit);
plot(x, y, "bo", xFit, yFit, "g-");
title("RTT test as a function of probe's payload (server delay: " + rtt_test(1, 2) + "s)");
xlabel("probe payload (bytes)");
ylabel("RTT (milliseconds)");

subplot(1, 2, 2);
x = thput_test(:, 1);
y = thput_test(:, 3);
c = polyfit(x, y, 1);
xFit = linspace(min(x), max(x), 100);
yFit = polyval(c, xFit);
plot(x, y, "ro", xFit, yFit, "g-");
title("Throughput test as a function of message size (server delay: " + thput_test(1, 2) + "s)");
xlabel("message size (kilobit)");
ylabel("throughput (kilobit/seconds)");

saveas(plots, "plots.png");
