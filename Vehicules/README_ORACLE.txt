SMART FIRE STATION — Qt + Oracle integration
================================================

This project uses Qt SQL with the QOCI Oracle driver.

1. Run the SQL schema
   - Open database(1).sql in Oracle SQL Developer.
   - Connect with the Oracle schema/user that will own the tables.
   - Run the script using F5 / Run Script.
   - The schema creates PERSONNEL, VEHICULE, MISSION, MISSION_PERSONNEL,
     MISSION_VEHICULE, sequences, triggers, and sample rows.

2. Open the Qt project
   - Open Vehicules.pro in Qt Creator.
   - Select a Qt kit matching the installed Oracle QOCI plugin architecture.
   - Run qmake / Configure Project, then build and run.

3. First launch
   - The application asks for Oracle host, port, SID/service, username, and password.
   - Defaults are localhost, 1521, and XE. Change them to match your Oracle setup.
   - The password is entered at runtime and is not saved in the project.
   - If the QOCI plugin is missing, or the connection is cancelled/fails, the app
     opens with demo data. Changes made in demo mode are not saved to Oracle.

4. QOCI prerequisites
   - Qt SQL module and the QOCI driver plugin must be installed for your exact Qt kit.
   - Oracle Instant Client must be installed and available to the application.
   - 32-bit/64-bit architecture and compiler/Qt kit must match.
   - If connection fails, read the error message and verify the service name and
     that the Oracle listener is running.

Database mapping notes
----------------------
- Oracle numeric IDs are shown as P001/P002... and V001/V002... in the UI.
- The SQL schema does not have a password column, so the UI password field is not
  persisted to Oracle.
- Mission counts are calculated from MISSION_PERSONNEL and MISSION.
- UI labels are mapped to schema values: Citerne <-> Camion-citerne and
  Bon <-> Operationnel.
