-- ============================================================================
-- Sample Demo Dataset for Flight Scheduling & Airport Management System
-- Run after schema.sql
-- ============================================================================

USE flight_ops;

-- 1. Admins
-- Demo Login 1: username = admin     | password = Admin@123
-- Demo Login 2: username = opslead   | password = Ops@2026
INSERT INTO admins (username, password_hash, full_name, role) VALUES
('admin', 'e86f78a8a3caf0b60d8e74e5942aa6d86dc150cd3c03338aef25b7d2d7e3acc7', 'Capt. Aarav Sharma', 'SUPER_ADMIN'),
('opslead', 'e8be19d5ea9a6895e1a86daf2c0b92b5acfddfa942cef032619ae97eff32beb3', 'Priya Nair', 'OPERATIONS_MANAGER');

-- 2. Airports
INSERT INTO airports (iata_code, icao_code, name, city, country, timezone, status) VALUES
('DEL', 'VIDP', 'Indira Gandhi International Airport', 'New Delhi', 'India', 'Asia/Kolkata', 'OPERATIONAL'),
('BOM', 'VABB', 'Chhatrapati Shivaji Maharaj International Airport', 'Mumbai', 'India', 'Asia/Kolkata', 'OPERATIONAL'),
('BLR', 'VOBL', 'Kempegowda International Airport', 'Bengaluru', 'India', 'Asia/Kolkata', 'OPERATIONAL'),
('DXB', 'OMDB', 'Dubai International Airport', 'Dubai', 'United Arab Emirates', 'Asia/Dubai', 'OPERATIONAL'),
('SIN', 'WSSS', 'Singapore Changi Airport', 'Singapore', 'Singapore', 'Asia/Singapore', 'OPERATIONAL'),
('LHR', 'EGLL', 'London Heathrow Airport', 'London', 'United Kingdom', 'Europe/London', 'OPERATIONAL');

-- 3. Runways
INSERT INTO runways (airport_id, runway_code, length_meters, surface_type, status) VALUES
(1, '11R/29L', 4430, 'ASPHALT', 'ACTIVE'),
(1, '10/28',   3810, 'ASPHALT', 'ACTIVE'),
(2, '09/27',   3448, 'ASPHALT', 'ACTIVE'),
(2, '14/32',   2871, 'CONCRETE', 'ACTIVE'),
(3, '09L/27R', 4000, 'ASPHALT', 'ACTIVE'),
(3, '09R/27L', 4000, 'CONCRETE', 'ACTIVE'),
(4, '12L/30R', 4000, 'ASPHALT', 'ACTIVE'),
(4, '12R/30L', 4447, 'ASPHALT', 'ACTIVE'),
(5, '02L/20R', 4000, 'ASPHALT', 'ACTIVE'),
(5, '02C/20C', 4000, 'ASPHALT', 'ACTIVE'),
(6, '09L/27R', 3902, 'ASPHALT', 'ACTIVE'),
(6, '09R/27L', 3658, 'ASPHALT', 'ACTIVE');

-- 4. Gates
INSERT INTO gates (airport_id, terminal, gate_code, gate_type, status) VALUES
(1, 'T3', 'A1', 'DOMESTIC', 'AVAILABLE'),
(1, 'T3', 'A2', 'DOMESTIC', 'AVAILABLE'),
(1, 'T3', 'B10', 'INTERNATIONAL', 'AVAILABLE'),
(2, 'T2', 'G4', 'DOMESTIC', 'AVAILABLE'),
(2, 'T2', 'G8', 'INTERNATIONAL', 'AVAILABLE'),
(3, 'T1', 'C2', 'DOMESTIC', 'AVAILABLE'),
(3, 'T2', 'D5', 'INTERNATIONAL', 'AVAILABLE'),
(4, 'T3', 'F12', 'WIDEBODY', 'AVAILABLE'),
(4, 'T3', 'F14', 'INTERNATIONAL', 'AVAILABLE'),
(5, 'T3', 'E3', 'WIDEBODY', 'AVAILABLE'),
(5, 'T1', 'E9', 'INTERNATIONAL', 'AVAILABLE'),
(6, 'T5', 'H2', 'WIDEBODY', 'AVAILABLE'),
(6, 'T2', 'K6', 'INTERNATIONAL', 'AVAILABLE');

-- 5. Aircraft
INSERT INTO aircraft (registration_no, model, manufacturer, total_seats, range_km, status, current_airport_id) VALUES
('VT-ANB', 'Boeing 787-8 Dreamliner', 'Boeing', 256, 13530, 'AVAILABLE', 1),
('VT-EXU', 'Airbus A321neo', 'Airbus', 180, 7400, 'AVAILABLE', 1),
('VT-IGR', 'Airbus A320neo', 'Airbus', 180, 6300, 'AVAILABLE', 2),
('VT-TNC', 'Boeing 737 MAX 8', 'Boeing', 174, 6570, 'IN_FLIGHT', 3),
('A6-EEO', 'Airbus A380-800', 'Airbus', 489, 14800, 'AVAILABLE', 4),
('9V-SMW', 'Airbus A350-900', 'Airbus', 253, 15000, 'AVAILABLE', 5),
('G-XWBA', 'Airbus A350-1000', 'Airbus', 331, 16100, 'AVAILABLE', 6),
('VT-KGO', 'Boeing 777-300ER', 'Boeing', 342, 13650, 'MAINTENANCE', 1);

-- 6. Flights
INSERT INTO flights (
    flight_number, airline_name, origin_airport_id, destination_airport_id,
    aircraft_id, departure_gate_id, arrival_gate_id, departure_runway_id, arrival_runway_id,
    scheduled_departure, scheduled_arrival, duration_minutes, priority_level, base_fare, status, remarks
) VALUES
('AI-101', 'Air India', 1, 6, 1, 3, 12, 1, 11,
 '2026-11-16 02:30:00', '2026-11-16 11:45:00', 555, 2, 720.00, 'SCHEDULED', 'Non-stop flagship service'),
('6E-204', 'IndiGo', 1, 2, 2, 1, 4, 2, 3,
 '2026-11-16 06:00:00', '2026-11-16 08:15:00', 135, 3, 115.00, 'BOARDING', 'On-time morning shuttle'),
('6E-519', 'IndiGo', 2, 3, 3, 4, 6, 3, 5,
 '2026-11-16 09:30:00', '2026-11-16 11:15:00', 105, 3, 98.00, 'SCHEDULED', 'Morning commuter flight'),
('EK-511', 'Emirates', 1, 4, 5, 3, 8, 1, 7,
 '2026-11-16 10:15:00', '2026-11-16 13:45:00', 210, 2, 340.00, 'SCHEDULED', 'Widebody A380 rotation'),
('SQ-403', 'Singapore Airlines', 1, 5, 6, 3, 10, 2, 9,
 '2026-11-16 13:00:00', '2026-11-16 18:40:00', 340, 2, 410.00, 'SCHEDULED', 'Afternoon international leg'),
('AI-805', 'Air India', 3, 1, 4, 6, 2, 5, 2,
 '2026-11-16 07:15:00', '2026-11-16 10:00:00', 165, 3, 130.00, 'IN_AIR', 'Cruising FL360'),
('BA-142', 'British Airways', 6, 1, 7, 13, 3, 12, 1,
 '2026-11-16 14:20:00', '2026-11-16 23:10:00', 530, 1, 780.00, 'DELAYED', 'Diplomatic priority clearance; 25 min ATC hold'),
('AI-409', 'Air India', 2, 1, 3, 4, 1, 4, 1,
 '2026-11-16 16:00:00', '2026-11-16 18:10:00', 130, 3, 120.00, 'CANCELLED', 'Cancelled due to crew roster realignment');

-- 7. Passengers
INSERT INTO passengers (full_name, email, phone, passport_number, nationality, date_of_birth) VALUES
('Rohan Verma', 'rohan.verma@example.com', '+91-9811122233', 'Z4829104', 'Indian', '1994-05-14'),
('Ananya Iyer', 'ananya.iyer@example.com', '+91-9820033445', 'P9182736', 'Indian', '1998-09-22'),
('Liam OConnor', 'liam.oconnor@example.co.uk', '+44-7700900123', '532819044', 'British', '1987-11-03'),
('Sophia Chen', 'sophia.chen@example.sg', '+65-81234567', 'E7654321K', 'Singaporean', '1992-02-18'),
('Tariq Al-Mansoor', 'tariq.mansoor@example.ae', '+971-501234567', 'N8834120', 'Emirati', '1985-07-29'),
('Meera Krishnan', 'meera.krishnan@example.com', '+91-9845098765', 'M1029384', 'Indian', '2001-12-09'),
('Vikramaditya Rao', 'vikram.rao@example.com', '+91-9900112233', 'K7748392', 'Indian', '1990-03-30'),
('Elena Rostova', 'elena.rostova@example.eu', '+44-7911223344', '759201843', 'British', '1996-08-11');

-- 8. Bookings
INSERT INTO bookings (booking_reference, flight_id, passenger_id, seat_number, seat_class, fare_paid, booking_status, cancelled_at) VALUES
('PNR8X91A', 1, 1, '04A', 'BUSINESS', 1440.00, 'CONFIRMED', NULL),
('PNR7B22C', 1, 3, '14C', 'ECONOMY',   720.00, 'CONFIRMED', NULL),
('PNR5K40D', 2, 2, '08A', 'ECONOMY',   115.00, 'CONFIRMED', NULL),
('PNR3M19E', 2, 6, '08B', 'ECONOMY',   115.00, 'CONFIRMED', NULL),
('PNR9Q77F', 2, 7, '02A', 'BUSINESS',  230.00, 'CANCELLED', '2026-11-15 09:30:00'),
('PNR4W61G', 3, 2, '11F', 'ECONOMY',    98.00, 'CONFIRMED', NULL),
('PNR2L85H', 4, 5, '01A', 'FIRST',    1020.00, 'CONFIRMED', NULL),
('PNR6N33J', 4, 1, '18D', 'ECONOMY',   340.00, 'CONFIRMED', NULL),
('PNR1V58K', 5, 4, '06C', 'BUSINESS',  820.00, 'CONFIRMED', NULL),
('PNR8Z12M', 6, 7, '10A', 'ECONOMY',   130.00, 'CONFIRMED', NULL),
('PNR4T90P', 8, 8, '15E', 'ECONOMY',   120.00, 'CANCELLED', '2026-11-15 08:00:00');
