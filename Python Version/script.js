/* =====================================================================
   KBC Advanced Edition – script.js
   Full game logic + SQLite DB integration via REST API
   ===================================================================== */

// ── Constants ──────────────────────────────────────────────────────────
const API = "http://localhost:8000/api";

const PRIZE_LADDER = [
    { level:  1, amount:      1000, safe: false },
    { level:  2, amount:      2000, safe: false },
    { level:  3, amount:      3000, safe: false },
    { level:  4, amount:      5000, safe: false },
    { level:  5, amount:     10000, safe: true  },
    { level:  6, amount:     20000, safe: false },
    { level:  7, amount:     40000, safe: false },
    { level:  8, amount:     80000, safe: false },
    { level:  9, amount:    160000, safe: false },
    { level: 10, amount:    320000, safe: true  },
    { level: 11, amount:    640000, safe: false },
    { level: 12, amount:   1250000, safe: false },
    { level: 13, amount:   2500000, safe: false },
    { level: 14, amount:   5000000, safe: false },
    { level: 15, amount:  10000000, safe: true  },
];

// ── State ───────────────────────────────────────────────────────────────
let player              = { name:"", age:"", location:"", occupation:"" };
let gameDeck            = [];
let backupDeck          = [];
let currentQIdx         = 0;
let timerInterval       = null;
let timeLeft            = 45;
let safePrize           = 0;
let currentPrize        = 0;
let acceptingAnswers    = false;
let used5050            = false;
let usedFlip            = false;
let usedDD              = false;
let isDoubleDip         = false;
let ddAttempts          = 0;
let questionsAnswered   = 0;
let gameOutcome         = "quit";

const TIMER_MAX         = 45;
const CIRCUMFERENCE     = 2 * Math.PI * 22;   // r=22

// ── Particle System ─────────────────────────────────────────────────────
(function initParticles() {
    const canvas = document.getElementById("particle-canvas");
    const ctx    = canvas.getContext("2d");
    let W, H, particles;

    const COLORS = ["#ffcb47", "#9b59b6", "#3498db", "#2ecc71", "#e74c3c"];
    const NUM    = 80;

    function resize() {
        W = canvas.width  = window.innerWidth;
        H = canvas.height = window.innerHeight;
    }

    function mkParticle() {
        const angle = Math.random() * Math.PI * 2;
        const speed = 0.15 + Math.random() * 0.4;
        return {
            x:   Math.random() * W,
            y:   Math.random() * H,
            r:   1 + Math.random() * 2.5,
            vx:  Math.cos(angle) * speed,
            vy:  Math.sin(angle) * speed,
            col: COLORS[Math.floor(Math.random() * COLORS.length)],
            alpha: 0.3 + Math.random() * 0.5,
        };
    }

    function init() {
        resize();
        particles = Array.from({ length: NUM }, mkParticle);
    }

    function draw() {
        ctx.clearRect(0, 0, W, H);

        // draw connections
        for (let i = 0; i < particles.length; i++) {
            for (let j = i + 1; j < particles.length; j++) {
                const p = particles[i], q = particles[j];
                const dx = p.x - q.x, dy = p.y - q.y;
                const dist = Math.sqrt(dx*dx + dy*dy);
                if (dist < 130) {
                    ctx.beginPath();
                    ctx.moveTo(p.x, p.y);
                    ctx.lineTo(q.x, q.y);
                    ctx.strokeStyle = `rgba(255,203,71,${0.06 * (1 - dist/130)})`;
                    ctx.lineWidth = 0.6;
                    ctx.stroke();
                }
            }
        }

        // draw particles
        particles.forEach(p => {
            ctx.beginPath();
            ctx.arc(p.x, p.y, p.r, 0, Math.PI * 2);
            ctx.fillStyle = p.col;
            ctx.globalAlpha = p.alpha;
            ctx.fill();
            ctx.globalAlpha = 1;

            p.x += p.vx;
            p.y += p.vy;
            if (p.x < -10) p.x = W + 10;
            if (p.x > W + 10) p.x = -10;
            if (p.y < -10) p.y = H + 10;
            if (p.y > H + 10) p.y = -10;
        });

        requestAnimationFrame(draw);
    }

    window.addEventListener("resize", resize);
    init();
    draw();
})();

// ── Hover light on option buttons ───────────────────────────────────────
document.querySelectorAll(".opt-btn").forEach(btn => {
    btn.addEventListener("mousemove", e => {
        const r = btn.getBoundingClientRect();
        btn.style.setProperty("--mx", ((e.clientX - r.left) / r.width * 100) + "%");
        btn.style.setProperty("--my", ((e.clientY - r.top)  / r.height * 100) + "%");
    });
});

// ── API Helpers ──────────────────────────────────────────────────────────
async function apiGet(endpoint) {
    try {
        const res = await fetch(`${API}/${endpoint}`);
        return await res.json();
    } catch { return null; }
}

async function apiPost(endpoint, data) {
    try {
        await fetch(`${API}/${endpoint}`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(data),
        });
    } catch { /* server may not be running */ }
}

// ── Home Screen Init ─────────────────────────────────────────────────────
async function initHome() {
    loadStats();
    loadHistory();
}

async function loadStats() {
    const s = await apiGet("stats");
    if (!s) return;
    document.getElementById("stat-games").textContent   = s.total_games;
    document.getElementById("stat-highest").textContent = "₹ " + formatINR(s.highest_prize);
    document.getElementById("stat-total").textContent   = "₹ " + formatINR(s.total_prize);
    document.getElementById("stat-wins").textContent    = s.wins;
}

async function loadHistory() {
    const list = await apiGet("history");
    const el   = document.getElementById("history-list");
    if (!list || list.length === 0) {
        el.innerHTML = `<p class="no-history">No games yet. Play your first game!</p>`;
        return;
    }
    el.innerHTML = list.map(g => `
        <div class="history-entry">
            <span class="he-name">${escHtml(g.player_name)}</span>
            <span class="he-prize">₹ ${formatINR(g.prize_won)}</span>
            <span class="he-meta">${g.played_at} &nbsp;·&nbsp; ${g.questions_answered} Q answered &nbsp;·&nbsp; ${g.location}</span>
            <span class="he-outcome ${g.outcome}">${g.outcome.toUpperCase()}</span>
        </div>
    `).join("");
}

function formatINR(n) {
    return Number(n).toLocaleString("en-IN");
}

function escHtml(s) {
    return s.replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
}

// ── Profile ───────────────────────────────────────────────────────────────
function fillDefault() {
    document.getElementById("p-name").value       = "Aakash";
    document.getElementById("p-age").value        = "19";
    document.getElementById("p-location").value   = "Pune";
    document.getElementById("p-occupation").value = "Student";
}

// ── Game Start ────────────────────────────────────────────────────────────
async function startGame() {
    const name = document.getElementById("p-name").value.trim();
    if (!name) { alert("Please enter your name!"); return; }

    player = {
        name:       name,
        age:        document.getElementById("p-age").value || "?",
        location:   document.getElementById("p-location").value || "Unknown",
        occupation: document.getElementById("p-occupation").value || "Unknown",
    };

    // reset state
    gameDeck = []; backupDeck = [];
    currentQIdx = 0; safePrize = 0; currentPrize = 0;
    questionsAnswered = 0; gameOutcome = "quit";
    used5050 = usedFlip = usedDD = isDoubleDip = false; ddAttempts = 0;

    // switch screens
    document.getElementById("home-screen").classList.remove("active");
    document.getElementById("game-screen").classList.add("active");

    document.getElementById("display-name").textContent = player.name;

    buildPrizeLadder();
    resetLifelineButtons();
    await loadQuestions();

    showModal("🎉", "Welcome!", `Welcome ${player.name}! You are a ${player.occupation} from ${player.location}. Let's see if you can win ₹ 1 Crore today!`, () => {
        loadQuestion(0);
    });
}

// ── Prize Ladder ─────────────────────────────────────────────────────────
function buildPrizeLadder() {
    const el = document.getElementById("prize-ladder");
    // Render from 15 down to 1 (column-reverse will flip visually)
    el.innerHTML = PRIZE_LADDER.slice().reverse().map(p => `
        <div class="prize-item ${p.safe ? "is-safe" : ""}" id="prize-${p.level}">
            <span>${p.level}</span>
            <span>₹ ${formatINR(p.amount)}</span>
        </div>
    `).join("");
}

// ── Question Loading ──────────────────────────────────────────────────────
function parseQFile(text) {
    const lines = text.replace(/\r/g, "").split("\n").map(l => l.trim()).filter(Boolean);
    const qs = [];
    for (let i = 0; i + 5 < lines.length; i += 7) {
        qs.push({
            q:   lines[i],
            a:   lines[i+1].replace(/^[Aa]\.\s*/, ""),
            b:   lines[i+2].replace(/^[Bb]\.\s*/, ""),
            c:   lines[i+3].replace(/^[Cc]\.\s*/, ""),
            d:   lines[i+4].replace(/^[Dd]\.\s*/, ""),
            ans: lines[i+5].toUpperCase(),
        });
    }
    return qs.sort(() => Math.random() - 0.5);
}

async function loadQuestions() {
    const [t1, t2, t3] = await Promise.all([
        fetch("questions1.txt").then(r=>r.text()),
        fetch("questions2.txt").then(r=>r.text()),
        fetch("questions3.txt").then(r=>r.text()),
    ]);
    const tier1 = parseQFile(t1);
    const tier2 = parseQFile(t2);
    const tier3 = parseQFile(t3);

    gameDeck = [...tier1.slice(0,5), ...tier2.slice(0,5), ...tier3.slice(0,5)];
    backupDeck = [tier1[5], tier2[5], tier3[5]].filter(Boolean);
}

// ── Load a Question ───────────────────────────────────────────────────────
function loadQuestion(idx) {
    if (idx >= 15) {
        endGame("won", 10000000);
        return;
    }
    currentQIdx = idx;
    const q = gameDeck[idx];
    const prize = PRIZE_LADDER[idx];

    // prize ladder highlight
    document.querySelectorAll(".prize-item").forEach(el => {
        el.classList.remove("active", "passed");
    });
    document.getElementById(`prize-${prize.level}`).classList.add("active");
    for (let i = 0; i < idx; i++) {
        const el = document.getElementById(`prize-${PRIZE_LADDER[i].level}`);
        if (el) el.classList.add("passed");
    }

    document.getElementById("q-number").textContent = `Q ${idx + 1} / 15`;
    document.getElementById("question-text").textContent = q.q;
    document.getElementById("display-prize").textContent = "₹ " + formatINR(prize.amount);

    ["A","B","C","D"].forEach(o => {
        const btn = document.getElementById(`opt-${o}`);
        btn.className = "opt-btn";
        btn.disabled  = false;
        document.getElementById(`text-${o}`).textContent = q[o.toLowerCase()];
    });

    isDoubleDip = false;
    acceptingAnswers = true;
    startTimer();
}

// ── Timer ─────────────────────────────────────────────────────────────────
function startTimer() {
    timeLeft = TIMER_MAX;
    updateTimerUI();
    clearInterval(timerInterval);
    timerInterval = setInterval(() => {
        timeLeft--;
        updateTimerUI();
        if (timeLeft <= 0) {
            clearInterval(timerInterval);
            acceptingAnswers = false;
            endGame("timeout", safePrize);
        }
    }, 1000);
}

function updateTimerUI() {
    const pct      = timeLeft / TIMER_MAX;
    const offset   = CIRCUMFERENCE * (1 - pct);
    const valEl    = document.getElementById("timer-val");
    const progEl   = document.getElementById("timer-progress");

    valEl.textContent          = timeLeft;
    progEl.style.strokeDashoffset = offset;
    if (timeLeft <= 10) {
        progEl.classList.add("danger");
        valEl.style.color = "var(--danger)";
    } else {
        progEl.classList.remove("danger");
        valEl.style.color = "";
    }
}

// ── Answer Selection ───────────────────────────────────────────────────────
function selectOption(opt) {
    if (!acceptingAnswers) return;

    const btn = document.getElementById(`opt-${opt}`);
    btn.classList.add("state-selected");

    // Disable lifelines while evaluating
    document.querySelectorAll(".lifeline-btn").forEach(b => b.disabled = true);

    if (isDoubleDip) {
        ddAttempts--;
        evaluateAnswer(opt, btn, true);
    } else {
        clearInterval(timerInterval);
        acceptingAnswers = false;
        setTimeout(() => evaluateAnswer(opt, btn, false), 1400);
    }
}

function evaluateAnswer(opt, btn, inDD) {
    const correct = gameDeck[currentQIdx].ans;

    if (opt === correct) {
        btn.classList.replace("state-selected", "state-correct");
        playSound("correct.wav");

        const prizeEntry = PRIZE_LADDER[currentQIdx];
        currentPrize = prizeEntry.amount;
        questionsAnswered++;
        if (prizeEntry.safe) safePrize = prizeEntry.amount;

        if (currentQIdx === 14) {
            // Won the jackpot!
            setTimeout(() => endGame("won", 10000000), 2500);
        } else {
            setTimeout(() => {
                resetLifelineButtons();
                loadQuestion(currentQIdx + 1);
            }, 2500);
        }
    } else {
        btn.classList.replace("state-selected", "state-wrong");
        playSound("Wrong.wav");

        if (inDD && ddAttempts > 0) {
            btn.disabled = true;
            acceptingAnswers = true;      // second attempt
        } else {
            document.getElementById(`opt-${correct}`).classList.add("state-correct");
            setTimeout(() => endGame("wrong", safePrize), 2500);
        }
    }
}

// ── Lifelines ───────────────────────────────────────────────────────────────
function resetLifelineButtons() {
    document.getElementById("ll-5050").disabled = used5050;
    document.getElementById("ll-flip").disabled  = usedFlip;
    document.getElementById("ll-dd").disabled    = usedDD;
}

function useLifeline(type) {
    if (!acceptingAnswers) return;

    if (type === "5050") {
        if (used5050) return;
        used5050 = true;
        document.getElementById("ll-5050").disabled = true;
        const correct = gameDeck[currentQIdx].ans;
        const wrong   = ["A","B","C","D"].filter(o => o !== correct).sort(() => Math.random()-0.5);
        document.getElementById(`opt-${wrong[0]}`).classList.add("state-hidden");
        document.getElementById(`opt-${wrong[1]}`).classList.add("state-hidden");
    }
    else if (type === "flip") {
        if (usedFlip) return;
        usedFlip = true;
        document.getElementById("ll-flip").disabled = true;
        const tier = Math.floor(currentQIdx / 5);
        if (backupDeck[tier]) {
            clearInterval(timerInterval);
            gameDeck[currentQIdx] = backupDeck[tier];
            showModal("🔄", "Question Flipped!", "Your question has been swapped. Good luck!", () => loadQuestion(currentQIdx));
        }
    }
    else if (type === "dd") {
        if (usedDD) return;
        usedDD = true;
        isDoubleDip = true;
        ddAttempts  = 2;
        document.getElementById("ll-dd").disabled    = true;
        document.getElementById("ll-5050").disabled  = true;
        document.getElementById("ll-flip").disabled  = true;
        showModal("x2", "Double Dip Activated!", "You now have 2 attempts. Lifelines are locked. You cannot quit.", () => {});
    }
}

// ── Quit ─────────────────────────────────────────────────────────────────
function quitGame() {
    if (isDoubleDip) {
        showModal("🔒", "Locked!", "You cannot quit while Double Dip is active.", () => {});
        return;
    }
    clearInterval(timerInterval);
    endGame("quit", currentPrize);
}

// ── End Game ──────────────────────────────────────────────────────────────
async function endGame(outcome, finalPrize) {
    gameOutcome = outcome;
    acceptingAnswers = false;
    clearInterval(timerInterval);

    // Save to DB
    await apiPost("save", {
        player_name:         player.name,
        location:            player.location,
        occupation:          player.occupation,
        prize_won:           finalPrize,
        questions_answered:  questionsAnswered,
        outcome:             outcome,
    });

    const icons   = { won:"🏆", wrong:"❌", timeout:"⏰", quit:"🏃" };
    const titles  = { won:"CROREPATI! 🎉", wrong:"Wrong Answer", timeout:"Time's Up!", quit:"You Quit" };
    const msgs    = {
        won:     `Congratulations ${player.name}! You've won ₹ 1,00,00,000! You are a CROREPATI! 🎉🏆`,
        wrong:   `Wrong answer! The correct option was shown. You take home ₹ ${formatINR(finalPrize)}.`,
        timeout: `You ran out of time! You take home ₹ ${formatINR(finalPrize)}.`,
        quit:    `You chose to quit. You take home ₹ ${formatINR(finalPrize)}. Well played!`,
    };

    playSound(outcome === "won" ? "correct.wav" : "Wrong.wav");

    showModal(icons[outcome] || "🎯", titles[outcome], msgs[outcome], () => goHome());
}

function goHome() {
    document.getElementById("game-screen").classList.remove("active");
    document.getElementById("home-screen").classList.add("active");
    loadStats();
    loadHistory();
}

// ── Modal ─────────────────────────────────────────────────────────────────
let _modalCb = null;

function showModal(icon, title, desc, cb) {
    document.getElementById("modal-icon").textContent  = icon;
    document.getElementById("modal-title").textContent = title;
    document.getElementById("modal-desc").textContent  = desc;
    document.getElementById("modal").classList.add("open");
    _modalCb = cb;
}

function closeModal() {
    document.getElementById("modal").classList.remove("open");
    if (_modalCb) { const f = _modalCb; _modalCb = null; f(); }
}

// ── Audio ────────────────────────────────────────────────────────────────
function playSound(file) {
    try { new Audio(file).play(); } catch {}
}

// ── Boot ─────────────────────────────────────────────────────────────────
initHome();
