// script.js
console.clear();
console.log("Responsive Clock JS LOADED:", new Date().toLocaleString());

window.onerror = function (message, source, lineno, colno, error) {
	console.error(
		"Global JS Error:",
		message,
		"in",
		source,
		"line",
		lineno,
		"col",
		colno,
		error
	);
};

const animateAllDigits = true;
const EPSILON = 0.001; // Small value for float comparisons

function calcResponsiveClockSize() {
	const digits = 4;
	const clocksPerDigitColumn = 2;
	const totalHorizontalClocks = digits * clocksPerDigitColumn;
	const gapCountBetweenDigits = digits - 1;
	const clockGapValue = 8;
	const verticalClocksPerRow = 3;
	const viewportWidth = window.innerWidth;
	const viewportHeight = window.innerHeight;
	const paddingHPercent =
		parseFloat(
			getComputedStyle(document.documentElement).getPropertyValue(
				"--padding-horizontal"
			)
		) || 5;
	const paddingVPercent =
		parseFloat(
			getComputedStyle(document.documentElement).getPropertyValue(
				"--padding-vertical"
			)
		) || 5;
	const paddingHPixels = viewportWidth * (paddingHPercent / 100);
	const paddingVPixels = viewportHeight * (paddingVPercent / 100);
	const availableW =
		viewportWidth - 2 * paddingHPixels - gapCountBetweenDigits * clockGapValue;
	const availableH =
		viewportHeight -
		2 * paddingVPixels -
		(verticalClocksPerRow - 1) * clockGapValue;
	const maxClockSizeW = availableW / totalHorizontalClocks;
	const maxClockSizeH = availableH / verticalClocksPerRow;
	const clockSize = Math.max(
		20,
		Math.floor(Math.min(maxClockSizeW, maxClockSizeH))
	);
	document.documentElement.style.setProperty("--clock-size", clockSize + "px");
	document.documentElement.style.setProperty(
		"--clock-gap",
		clockGapValue + "px"
	);
}

window.addEventListener("resize", calcResponsiveClockSize);
window.addEventListener("DOMContentLoaded", () => {
	calcResponsiveClockSize();
	forceFullUIRefresh("DOMContentLoaded -> Avvio Iniziale");
	if (!customTimeActive) {
		if (intervalId) clearInterval(intervalId);
		intervalId = setInterval(tickRealTime, 1000);
	}
});

const hr1 = document.getElementById("hr-1");
const hr2 = document.getElementById("hr-2");
const mn1 = document.getElementById("mn-1");
const mn2 = document.getElementById("mn-2");

const clockConfig = { use24HourFormat: true };
let prevDigits = { h1: null, h2: null, m1: null, m2: null }; // Stores previous numeric digit values
let customTimeActive = false;
let customHours_24fmt = 0;
let customMinutes_val = 0;
let intervalId = null;
let lastMinuteProcessed = -1;

// Stores the current CUMULATIVE CSS rotation multipliers for each hand of each digit
const digitHandStates = {
	"hr-1": Array(12).fill(0),
	"hr-2": Array(12).fill(0),
	"mn-1": Array(12).fill(0),
	"mn-2": Array(12).fill(0)
};

const digitClockMap = {
	// Base orientations (0-3, or 2.6 for special) for each digit
	0: [2, 1, 2, 3, 0, 2, 0, 2, 0, 1, 0, 3],
	1: [2.6, 2.6, 2, 2, 2.6, 2.6, 0, 2, 2.6, 2.6, 0, 0],
	2: [1, 1, 2, 3, 1, 2, 0, 3, 0, 1, 3, 3],
	3: [1, 1, 2, 3, 1, 1, 0, 3, 1, 1, 0, 3],
	4: [2, 2, 2, 2, 0, 1, 0, 2, 2.6, 2.6, 0, 0],
	5: [1, 2, 3, 3, 0, 1, 3, 2, 1, 1, 0, 3],
	6: [1, 2, 3, 3, 0, 2, 3, 2, 0, 1, 0, 3],
	7: [1, 1, 3, 2, 2.6, 2.6, 0, 2, 2.6, 2.6, 0, 0],
	8: [2, 1, 2, 3, 2, 1, 2, 3, 0, 1, 0, 3],
	9: [2, 1, 2, 3, 0, 1, 0, 2, 1, 1, 0, 3]
};

function setAllDigitHands(digitEl, cumulativeMultiplierArray) {
	for (let i = 1; i <= 3; i++) {
		for (let j = 1; j <= 2; j++) {
			const handIndexBase = ((i - 1) * 2 + (j - 1)) * 2;
			digitEl.style.setProperty(
				`--r${i}c${j}-a`,
				cumulativeMultiplierArray[handIndexBase]
			);
			digitEl.style.setProperty(
				`--r${i}c${j}-b`,
				cumulativeMultiplierArray[handIndexBase + 1]
			);
		}
	}
}

// Calculates the next CW cumulative multiplier for a hand
function calculateNextCWMultiplier(
	current_mult,
	target_base_mult,
	force_spin_if_no_change = false
) {
	let k_min = Math.ceil((current_mult - target_base_mult - EPSILON) / 4.0);
	let next_mult = target_base_mult + 4.0 * k_min;

	if (next_mult < current_mult - EPSILON) {
		next_mult += 4.0;
	}

	if (force_spin_if_no_change && Math.abs(next_mult - current_mult) < EPSILON) {
		next_mult += 4.0;
	}
	return next_mult;
}

function updateDisplayDigits(h_24, m_val, minuteHasChanged) {
	let h1_digit, h2_digit;
	if (clockConfig.use24HourFormat) {
		h1_digit = Math.floor(h_24 / 10);
		h2_digit = h_24 % 10;
	} else {
		let hh_display = h_24 % 12;
		if (hh_display === 0) hh_display = 12;
		h1_digit = Math.floor(hh_display / 10);
		h2_digit = hh_display % 10;
	}
	const m1_digit = Math.floor(m_val / 10);
	const m2_digit = m_val % 10;

	const newDigitsState = {
		h1: h1_digit,
		h2: h2_digit,
		m1: m1_digit,
		m2: m2_digit
	};
	const digitElements = { h1: hr1, h2: hr2, m1: mn1, m2: mn2 };
	const totalHandTransitionDurationMs =
		parseFloat(
			getComputedStyle(document.documentElement).getPropertyValue(
				"--hand-transition-duration"
			)
		) * 1000 || 6000; // Default to 6s

	for (const key in newDigitsState) {
		const digitElement = digitElements[key];
		const currentDigitHandMultipliers = digitHandStates[digitElement.id];
		const newTargetDigitNumericValue = newDigitsState[key];
		const previousStoredDigitNumericValue = prevDigits[key];

		if (minuteHasChanged) {
			digitElement.classList.remove("digit-animating-two-phase");

			if (
				newTargetDigitNumericValue !== previousStoredDigitNumericValue ||
				previousStoredDigitNumericValue === null
			) {
				// CASO 1: La cifra cambia valore o è l'impostazione iniziale.
				const targetBaseOrientations = digitClockMap[newTargetDigitNumericValue];
				const nextCumulativeMultipliers = Array(12);
				for (let i = 0; i < 12; i++) {
					nextCumulativeMultipliers[i] = calculateNextCWMultiplier(
						currentDigitHandMultipliers[i],
						targetBaseOrientations[i],
						false
					);
				}
				digitElement.setAttribute(
					"data-digit",
					newTargetDigitNumericValue.toString()
				);
				setAllDigitHands(digitElement, nextCumulativeMultipliers);
				digitHandStates[digitElement.id] = [...nextCumulativeMultipliers];
			} else if (animateAllDigits) {
				// CASO 2: La cifra NON cambia valore, ma animateAllDigits è true.
				digitElement.classList.add("digit-animating-two-phase");
				const phaseDuration = totalHandTransitionDurationMs / 2; // Duration of each phase

				// --- Fase 1: Da attuale a casuale (sempre CW) ---
				const p1_targetCumulativeMultipliers = Array(12);
				for (let i = 0; i < 12; i++) {
					const current_mult = currentDigitHandMultipliers[i];
					// Per determinare l'orientamento visivo attuale, confrontiamo con i valori base possibili (0,1,2,3 e 2.6)
					let current_visual_base = -1;
					const possible_bases = [0, 1, 2, 3, 2.6];
					for (const base_val of possible_bases) {
						if (
							Math.abs((current_mult % 4) - (base_val % 4)) < EPSILON ||
							Math.abs(current_mult - base_val) < EPSILON
						) {
							// Special handling for 2.6 which doesn't fit neatly into modulo 4 for visual orientation
							if (
								Math.abs(current_mult - 2.6) < EPSILON ||
								Math.abs(current_mult - 2.6 + 4) < EPSILON ||
								Math.abs(current_mult - 2.6 - 4) < EPSILON
							) {
								current_visual_base = 2.6;
								break;
							} else if (Math.abs((current_mult % 4) - base_val) < EPSILON) {
								current_visual_base = base_val;
								break;
							}
						}
					}
					if (current_visual_base === -1) current_visual_base = current_mult % 4; // Fallback

					let target_random_base;
					do {
						target_random_base = Math.floor(Math.random() * 4); // 0,1,2,3
					} while (Math.abs(target_random_base - current_visual_base) < EPSILON);

					p1_targetCumulativeMultipliers[i] = calculateNextCWMultiplier(
						current_mult,
						target_random_base,
						true
					);
				}
				setAllDigitHands(digitElement, p1_targetCumulativeMultipliers);
				digitHandStates[digitElement.id] = [...p1_targetCumulativeMultipliers];

				// --- Fase 2: Da casuale (fine Fase 1) a corretto (sempre CW) ---
				// Rimosso il randomDelayOffset per una transizione più diretta
				const phaseTwoStartActualDelay = phaseDuration;

				setTimeout(() => {
					const finalTargetBaseOrientations =
						digitClockMap[newTargetDigitNumericValue];
					const p2_startCumulativeMultipliers = [
						...digitHandStates[digitElement.id]
					];
					const p2_targetCumulativeMultipliers = Array(12);
					for (let i = 0; i < 12; i++) {
						p2_targetCumulativeMultipliers[i] = calculateNextCWMultiplier(
							p2_startCumulativeMultipliers[i],
							finalTargetBaseOrientations[i],
							true
						);
					}
					digitElement.setAttribute(
						"data-digit",
						newTargetDigitNumericValue.toString()
					);
					setAllDigitHands(digitElement, p2_targetCumulativeMultipliers);
					digitHandStates[digitElement.id] = [...p2_targetCumulativeMultipliers];

					setTimeout(() => {
						digitElement.classList.remove("digit-animating-two-phase");
					}, phaseDuration + 50); // Buffer per il completamento della transizione CSS
				}, phaseTwoStartActualDelay);
			}
		}
		prevDigits[key] = newTargetDigitNumericValue;
	}
}

function tickRealTime() {
	if (customTimeActive) return;
	const now = new Date();
	const currentHours = now.getHours();
	const currentMinutes = now.getMinutes();
	if (currentMinutes !== lastMinuteProcessed || lastMinuteProcessed === -1) {
		updateDisplayDigits(currentHours, currentMinutes, true);
		lastMinuteProcessed = currentMinutes;
	}
}

function forceFullUIRefresh(context = "N/A") {
	console.log(`Forcing full UI refresh. Context: ${context}`);
	const now = customTimeActive
		? new Date(0, 0, 0, customHours_24fmt, customMinutes_val)
		: new Date();
	const h = now.getHours();
	const m = now.getMinutes();

	let h1_val, h2_val;
	if (clockConfig.use24HourFormat) {
		h1_val = Math.floor(h / 10);
		h2_val = h % 10;
	} else {
		let hh_display = h % 12;
		if (hh_display === 0) hh_display = 12;
		h1_val = Math.floor(hh_display / 10);
		h2_val = hh_display % 10;
	}
	const m1_val = Math.floor(m / 10);
	const m2_val = m % 10;

	const initialNumericStates = {
		h1: h1_val,
		h2: h2_val,
		m1: m1_val,
		m2: m2_val
	};
	const digitElements = { h1: hr1, h2: hr2, m1: mn1, m2: mn2 };

	for (const key in digitElements) {
		const digitEl = digitElements[key];
		const numericValue = initialNumericStates[key];
		const baseOrientations = digitClockMap[numericValue];

		digitHandStates[digitEl.id] = [...baseOrientations];
		setAllDigitHands(digitEl, baseOrientations);
		digitEl.setAttribute("data-digit", numericValue.toString());
		prevDigits[key] = numericValue;
	}
	lastMinuteProcessed = m;
}

// === Public API Functions (callable from browser console) ===
function setFixedTime(hours_24fmt, minutes_val) {
	const h = parseInt(hours_24fmt, 10);
	const m = parseInt(minutes_val, 10);
	if (isNaN(h) || h < 0 || h > 23 || isNaN(m) || m < 0 || m > 59) {
		console.error(
			"Invalid input for setFixedTime. Hours (0-23), Minutes (0-59)."
		);
		return;
	}
	if (intervalId) {
		clearInterval(intervalId);
		intervalId = null;
	}
	customTimeActive = true;
	customHours_24fmt = h;
	customMinutes_val = m;
	forceFullUIRefresh(`setFixedTime(${h}, ${m})`);
	console.log(
		`Clock time fixed to: ${String(h).padStart(2, "0")}:${String(m).padStart(
			2,
			"0"
		)}`
	);
}
function resumeRealTime() {
	if (!customTimeActive) {
		console.log("Real-time is already active.");
		return;
	}
	customTimeActive = false;
	if (intervalId) clearInterval(intervalId);
	forceFullUIRefresh("resumeRealTime");
	intervalId = setInterval(tickRealTime, 1000);
	console.log("Clock resumed to real-time.");
}
function setHourFormat(is24Hour) {
	clockConfig.use24HourFormat = !!is24Hour;
	forceFullUIRefresh(`setHourFormat(${is24Hour})`);
	console.log(`Hour format set to: ${is24Hour ? "24-hour" : "12-hour"}`);
}
function setTransitionSpeed(durationInSeconds) {
	const duration = parseFloat(durationInSeconds);
	if (typeof duration === "number" && duration >= 0) {
		document.documentElement.style.setProperty(
			"--hand-transition-duration",
			duration + "s"
		);
		console.log(`Hand transition duration set to: ${duration}s`);
	} else {
		console.error("Invalid duration for setTransitionSpeed.");
	}
}
function setHandTipStyle(style) {
	const rootStyle = document.documentElement.style;
	if (style === "round")
		rootStyle.setProperty("--hand-tip-radius", "calc(var(--hand-width) / 2)");
	else if (style === "square") rootStyle.setProperty("--hand-tip-radius", "0px");
	else console.error("Invalid tip style.");
}
function setHandBaseStyle(style) {
	const rootStyle = document.documentElement.style;
	if (style === "round")
		rootStyle.setProperty("--hand-base-radius", "calc(var(--hand-width) / 2)");
	else if (style === "square")
		rootStyle.setProperty("--hand-base-radius", "0px");
	else console.error("Invalid base style.");
}

window.setFixedTime = setFixedTime;
window.resumeRealTime = resumeRealTime;
window.setHourFormat = setHourFormat;
window.setTransitionSpeed = setTransitionSpeed;
window.setHandTipStyle = setHandTipStyle;
window.setHandBaseStyle = setHandBaseStyle;
