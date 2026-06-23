#!/usr/bin/env python3
"""Generate omen-OS-report.docx with no third-party deps.

A .docx is a zip of OOXML parts. We emit a minimal, valid WordprocessingML
document with a title, headings, body paragraphs, and bullet lists.
"""
import zipfile
from xml.sax.saxutils import escape

OUT = "omen-OS-report.docx"

# ---- document content as a small DSL: (kind, text) -------------------------
# kinds: title, subtitle, h1, p, bullet
DOC = [
    ("title", "omen OS"),
    ("subtitle", "An Operating System Built From Scratch"),
    ("subtitle", "BSIT 338 — Operating Systems · Summer 2026"),
    ("subtitle", "Aarohan Niraula · Bishesh Raut · Karan Tamang"),

    ("h1", "1. Introduction"),
    ("p", "We are a team of three students — Aarohan Niraula, Bishesh Raut, and "
          "Karan Tamang — completing this project for BSIT 338 (Operating "
          "Systems). Rather than configure or extend an existing operating "
          "system, we chose to build one of our own from the ground up, which "
          "we named omen OS."),
    ("p", "We chose this project because an operating system is the one piece "
          "of software almost every programmer relies on daily yet rarely "
          "understands from the inside. Reading about interrupts, paging, and "
          "context switching is very different from making a bare machine "
          "actually do them. By starting from nothing — a CPU that has just "
          "been handed control by the bootloader — and ending at a working "
          "interactive shell, we forced ourselves to learn how each layer of a "
          "real OS is built and why it exists. Everything in omen OS is code we "
          "wrote and understand, line by line, with no operating-system "
          "libraries underneath us."),
    ("p", "omen OS targets the 32-bit x86 architecture, runs entirely in "
          "protected mode, and is written in freestanding C with small amounts "
          "of hand-written assembly where the hardware demands it. It boots "
          "under GRUB via the multiboot standard and runs on the QEMU machine "
          "emulator as well as on real x86-compatible hardware."),

    ("h1", "2. Features"),
    ("p", "omen OS implements the full path from power-on to an interactive "
          "prompt. The major features are:"),
    ("bullet", "Boot and serial logging — a multiboot stub that GRUB loads, "
               "which verifies the boot magic and brings the kernel to life, "
               "with a serial port used for diagnostic output."),
    ("bullet", "VGA text-mode display — an 80x25 text driver with scrolling, "
               "colored output, a cursor, and destructive backspace handling."),
    ("bullet", "GDT and IDT — the Global Descriptor Table and Interrupt "
               "Descriptor Table that protected-mode x86 requires before any "
               "interrupt can be safely handled."),
    ("bullet", "PIC remapping and the PIT timer — the interrupt controller is "
               "remapped so hardware IRQs do not collide with CPU exceptions, "
               "and the Programmable Interval Timer drives the system clock at "
               "100 Hz."),
    ("bullet", "PS/2 keyboard and mouse drivers — interrupt-driven input, "
               "scancode translation, and a tracked mouse cursor with button "
               "state."),
    ("bullet", "Physical memory manager and paging — a bitmap allocator that "
               "tracks free physical frames, and page tables that map virtual "
               "to physical memory."),
    ("bullet", "Kernel heap (kmalloc) — dynamic kernel memory allocation on "
               "top of the physical frame allocator."),
    ("bullet", "Multithreading — kernel threads with a hand-written assembly "
               "context switch that saves and restores CPU state."),
    ("bullet", "Preemptive CPU scheduling — a round-robin scheduler driven by "
               "the timer interrupt, so even threads that never yield share "
               "the CPU fairly."),
    ("bullet", "Multiprocessing with isolated address spaces — each process "
               "gets its own page directory, so two processes can use the same "
               "virtual address and never see each other's memory."),
    ("bullet", "An interactive shell — a keyboard-driven command prompt with "
               "line editing and a command table. Commands include help, "
               "about, clear, echo, meminfo, and uptime, plus live demos "
               "(threads, sched, proc, mouse) that run each core feature on "
               "demand, and reboot."),

    ("h1", "3. System Specifications"),
    ("p", "omen OS is a 32-bit x86 operating system. Because it is small and "
          "makes no assumptions about modern hardware, its requirements are "
          "very modest. It is normally run inside the QEMU emulator, which can "
          "host it on any modern computer."),
    ("p", "Minimum requirements (to run omen OS):"),
    ("bullet", "CPU: any IA-32 (i686) compatible x86 processor."),
    ("bullet", "Memory: approximately 16 MB of RAM (the kernel identity-maps "
               "the low 16 MB at boot)."),
    ("bullet", "Display: a VGA-compatible text-mode display."),
    ("bullet", "Input: a PS/2-compatible keyboard and mouse."),
    ("bullet", "Firmware: a multiboot-compliant bootloader such as GRUB."),
    ("p", "Recommended environment (for development and demonstration):"),
    ("bullet", "A host PC running macOS or Linux."),
    ("bullet", "QEMU (qemu-system-i386) to emulate the target machine."),
    ("bullet", "The i686-elf GCC cross-compiler toolchain and NASM assembler "
               "to build the kernel."),
    ("bullet", "64 MB or more of emulated RAM for comfortable headroom, though "
               "the OS itself needs far less."),

    ("h1", "4. OS Specification"),
    ("p", "The following describes the internal design of omen OS."),
    ("bullet", "Architecture: 32-bit x86 (IA-32), protected mode, running "
               "entirely in ring 0 (kernel mode). There is no user mode, no "
               "filesystem, and no multi-core (SMP) support — the focus is on "
               "core OS mechanisms rather than completeness."),
    ("bullet", "Language and toolchain: freestanding C (gnu11) compiled with "
               "the i686-elf cross-compiler, plus NASM assembly for the boot "
               "stub, the context switch, and low-level table loading. No "
               "standard C library is used; even string helpers are written by "
               "hand."),
    ("bullet", "Boot: GRUB loads the kernel via the multiboot standard. The "
               "kernel verifies the multiboot magic value, then initializes "
               "each subsystem in order."),
    ("bullet", "Memory model: the physical memory manager tracks free 4 KB "
               "frames with a bitmap. Paging identity-maps the low 16 MB so "
               "kernel code and data are reachable, and a bump allocator "
               "provides the kernel heap."),
    ("bullet", "Concurrency: kernel threads each have a task control block and "
               "their own stack. A hand-written context switch swaps stacks "
               "and registers. The scheduler is preemptive round-robin, driven "
               "by the 100 Hz timer interrupt."),
    ("bullet", "Process isolation: each process has its own page directory that "
               "shares the kernel's low-memory mapping but maps process-private "
               "virtual addresses to private physical frames. Switching "
               "processes reloads the CR3 register, which also flushes the TLB "
               "so no stale translations leak between address spaces."),
    ("bullet", "User interface: an interactive shell implemented as a "
               "read-evaluate-print loop. It reads keystrokes from the keyboard "
               "driver, supports line editing, tokenizes input, and dispatches "
               "to commands through a static command table."),

    ("h1", "5. Challenges"),
    ("p", "Building an operating system from scratch surfaced a number of "
          "difficulties that are easy to underestimate from the outside:"),
    ("bullet", "Toolchain setup: getting a working i686-elf cross-compiler and "
               "linker configured correctly took real effort. A normal host "
               "compiler cannot produce a freestanding kernel, and small "
               "misconfigurations produced binaries that simply would not "
               "boot."),
    ("bullet", "Debugging without a debugger: when something goes wrong early "
               "in boot the machine often triple-faults and resets instantly, "
               "with no error message. We leaned heavily on serial-port log "
               "markers to narrow down exactly which initialization step "
               "failed."),
    ("bullet", "The descriptor tables: the GDT and IDT are unforgiving. A "
               "single wrong bit in a descriptor, or an off-by-one in the table "
               "limit, causes immediate faults, so these had to be built and "
               "verified very carefully."),
    ("bullet", "The context switch: writing the thread context switch in "
               "assembly meant manually saving and restoring registers and "
               "laying out the stack exactly as the CPU and our scheduler "
               "expected. Any mismatch corrupted execution in ways that were "
               "hard to trace."),
    ("bullet", "Address-space isolation: making two processes share a virtual "
               "address but see different memory required correctly building "
               "per-process page directories and remembering to flush the TLB "
               "on every CR3 switch — otherwise stale mappings caused one "
               "process to read another's data."),
    ("bullet", "Input and display edge cases: synchronizing the 3-byte PS/2 "
               "mouse packets, and handling backspace correctly when the "
               "cursor needs to step back across a wrapped line in the VGA text "
               "buffer, both required more care than expected."),
    ("bullet", "No standard library: with no libc available we had to write "
               "our own small utilities, such as string comparison and number "
               "formatting, and verify behavior through a headless automated "
               "test harness."),

    ("h1", "6. Conclusion"),
    ("p", "omen OS achieves what we set out to do: it boots a bare x86 machine, "
          "brings up the core subsystems of a real operating system — "
          "interrupts, memory management, paging, multithreading, preemptive "
          "scheduling, and isolated processes — and presents the user with an "
          "interactive shell that can exercise each of those features on "
          "demand. Every layer was written and understood by the team."),
    ("p", "Beyond the running software, the most valuable outcome was the "
          "understanding we gained. Concepts that were previously abstract — "
          "why the GDT exists, how a context switch actually works, what the "
          "CR3 register does, how preemption is really enforced — became "
          "concrete because we had to make the hardware perform them ourselves."),
    ("p", "There is plenty of room to grow omen OS further: adding a user mode "
          "with system calls, a real filesystem, dynamic memory freeing in the "
          "heap, and support for multiple CPU cores would each be natural next "
          "steps. As a foundation for learning how operating systems work, "
          "however, the project has already succeeded."),
]

# ---- OOXML emitters --------------------------------------------------------
def run(text, bold=False, size=None, color=None):
    rpr = ""
    if bold or size or color:
        parts = []
        if bold: parts.append('<w:b/>')
        if color: parts.append(f'<w:color w:val="{color}"/>')
        if size: parts.append(f'<w:sz w:val="{size}"/><w:szCs w:val="{size}"/>')
        rpr = "<w:rPr>" + "".join(parts) + "</w:rPr>"
    return f'<w:r>{rpr}<w:t xml:space="preserve">{escape(text)}</w:t></w:r>'

def para(kind, text):
    if kind == "title":
        return ('<w:p><w:pPr><w:spacing w:before="120" w:after="60"/>'
                '<w:jc w:val="center"/></w:pPr>'
                + run(text, bold=True, size="56", color="1F3864") + '</w:p>')
    if kind == "subtitle":
        return ('<w:p><w:pPr><w:spacing w:after="40"/><w:jc w:val="center"/></w:pPr>'
                + run(text, size="24", color="404040") + '</w:p>')
    if kind == "h1":
        return ('<w:p><w:pPr><w:spacing w:before="280" w:after="120"/>'
                '<w:pBdr><w:bottom w:val="single" w:sz="6" w:space="2" '
                'w:color="1F3864"/></w:pBdr></w:pPr>'
                + run(text, bold=True, size="32", color="1F3864") + '</w:p>')
    if kind == "bullet":
        return ('<w:p><w:pPr><w:numPr><w:ilvl w:val="0"/><w:numId w:val="1"/>'
                '</w:numPr><w:spacing w:after="80"/></w:pPr>'
                + run(text, size="22") + '</w:p>')
    # normal paragraph
    return ('<w:p><w:pPr><w:spacing w:after="140" w:line="276" '
            'w:lineRule="auto"/><w:jc w:val="both"/></w:pPr>'
            + run(text, size="22") + '</w:p>')

body = "".join(para(k, t) for k, t in DOC)

document_xml = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    '<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">'
    '<w:body>' + body +
    '<w:sectPr><w:pgSz w:w="12240" w:h="15840"/>'
    '<w:pgMar w:top="1440" w:bottom="1440" w:left="1440" w:right="1440"/>'
    '</w:sectPr></w:body></w:document>'
)

numbering_xml = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    '<w:numbering xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">'
    '<w:abstractNum w:abstractNumId="0">'
    '<w:lvl w:ilvl="0"><w:start w:val="1"/><w:numFmt w:val="bullet"/>'
    '<w:lvlText w:val="•"/><w:lvlJc w:val="left"/>'
    '<w:pPr><w:ind w:left="720" w:hanging="360"/></w:pPr>'
    '<w:rPr><w:rFonts w:ascii="Symbol" w:hAnsi="Symbol"/></w:rPr></w:lvl>'
    '</w:abstractNum>'
    '<w:num w:numId="1"><w:abstractNumId w:val="0"/></w:num>'
    '</w:numbering>'
)

content_types = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
    '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
    '<Default Extension="xml" ContentType="application/xml"/>'
    '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
    '<Override PartName="/word/numbering.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml"/>'
    '</Types>'
)

root_rels = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
    '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>'
    '</Relationships>'
)

doc_rels = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
    '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering" Target="numbering.xml"/>'
    '</Relationships>'
)

with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("[Content_Types].xml", content_types)
    z.writestr("_rels/.rels", root_rels)
    z.writestr("word/document.xml", document_xml)
    z.writestr("word/numbering.xml", numbering_xml)
    z.writestr("word/_rels/document.xml.rels", doc_rels)

print("wrote", OUT)
