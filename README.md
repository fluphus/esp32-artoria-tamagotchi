[English](README.md) | [简体中文](README_zh.md) | [日本語](README_ja.md)
# 👑 Artoria Tamagotchi (Fate/Grand Order Virtual Pet)

A Tamagotchi-like virtual pet project powered by the **ESP32-S3 (N16R8)**. 

Raise your own Artoria! Start your journey with Saber Lily and guide her growth. Depending on your care and interactions, she can evolve into various classes including Archer, Lancer, Rider, and more.

## ✨ Features (Core Logic Implemented)
* **Evolution System:** Multiple growth routes based on pet stats (Health, Seriousness, etc.).
* **Time & Day Management:** Built-in internal clock to handle daily resets and penalties for missing meals.
* **Persistent Storage:** Auto save/load system with checksum validation (prevents data corruption on power loss).
* **Interactive Actions:** Feed, Poke, and monitor status.

## 🚧 Current Project Status
**Work In Progress (Backend Only)**

Currently, the project consists of the **core logic and state machine**. 
* ❌ No GUI / Screen support yet.
* ✅ Fully functional via **Serial Port Debugging**. 

You can interact with Artoria by sending commands through the serial monitor. Hardware UI (screen & buttons) integration is planned for future updates.

## 🛠️ Hardware Requirement
* ESP32-S3 Development Board (N16R8 recommended)
* (Future) Display module & Buttons
