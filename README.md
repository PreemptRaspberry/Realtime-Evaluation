# Realtime-Evaluation
Determination of the real-time capabilities of the Linux OS on a Raspberry Pi - A research at Salzburg University of Applied Sciences
-----

Immer mehr industrielle Anwendungen beruhen auf Echtzeitfähigkeit. Echtzeitfähige Systeme stellen sicher, dass bestimmte zeitliche Vorgaben eingehalten werden (Deadline). Diese muss auch bei voller Auslastung, also im "Worst-Case" eingehalten werden. Allerdings ist speziell dafür entwickelte Hardware mit hohen Anschaffungskosten verbunden und somit nicht für alle Zwecke geeignet. Die Frage, ob ein günstiger Einplatinencomputer wie etwa der Raspberry Pi dabei teure Sonderhardware ablösen kann, wurde im Bachelorprojekt von Valentin Brandner, Jonas Gasteiger und Daniel Willi untersucht.

Nicht nur der Preis, sondern verschiedene weitere Aspekte sprechen für die Verwendung des Raspberry Pis. So ist es zum Beispiel sehr leicht weitere Peripherie mittels der GPIO-Schnittstelle anzubinden. Um dem Linuxkernel Echtzeitfähigkeit zu verleihen existieren verschiedene Ansätze - im Rahmen dieser Arbeit wurde dabei der PREEMPT_RT-Patch genauer untersucht.

Mittels eines Mikrocontrollers, Softwaretools und eigens dafür entwickelten Testprogrammen wurde das originale mit dem gepatchten Betriebssystem am Raspberry Pi 3B+ und am Raspberry Pi 4B verglichen.

Untersucht wurden Verzögerungen bei der Prozesseinteilung (Scheduler), sowie Verzögerungen die beim Erkennen und Abarbeiten von Signalen an der GPIO-Schnittstelle auftreten.

Anhand der Ergebnisse der verschiedenen Tests konnten abschließend Rückschlüsse auf die Eignung des Einplatinencomputers in der industriellen Verwendung gezogen werden.
