-- ============================================================================
-- Flight Scheduling & Airport Management System (flight_ops)
-- Complete Relational Database Schema for MySQL 8.0+
-- ============================================================================

DROP DATABASE IF EXISTS flight_ops;
CREATE DATABASE flight_ops
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

USE flight_ops;

-- ----------------------------------------------------------------------------
-- 1. ADMINS TABLE
-- ----------------------------------------------------------------------------
CREATE TABLE admins (
    admin_id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password_hash CHAR(64) NOT NULL COMMENT 'SHA-256 hex digest',
    full_name VARCHAR(100) NOT NULL,
    role ENUM('SUPER_ADMIN', 'OPERATIONS_MANAGER', 'DISPATCHER') NOT NULL DEFAULT 'OPERATIONS_MANAGER',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

-- ----------------------------------------------------------------------------
-- 2. AIRPORTS TABLE
-- ----------------------------------------------------------------------------
CREATE TABLE airports (
    airport_id INT AUTO_INCREMENT PRIMARY KEY,
    iata_code CHAR(3) NOT NULL UNIQUE,
    icao_code CHAR(4) NOT NULL UNIQUE,
    name VARCHAR(150) NOT NULL,
    city VARCHAR(100) NOT NULL,
    country VARCHAR(100) NOT NULL,
    timezone VARCHAR(64) NOT NULL DEFAULT 'UTC',
    status ENUM('OPERATIONAL', 'RESTRICTED', 'CLOSED') NOT NULL DEFAULT 'OPERATIONAL',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT chk_iata_len CHECK (CHAR_LENGTH(iata_code) = 3),
    CONSTRAINT chk_icao_len CHECK (CHAR_LENGTH(icao_code) = 4)
) ENGINE=InnoDB;

CREATE INDEX idx_airports_city ON airports(city);

-- ----------------------------------------------------------------------------
-- 3. RUNWAYS TABLE
-- ----------------------------------------------------------------------------
CREATE TABLE runways (
    runway_id INT AUTO_INCREMENT PRIMARY KEY,
    airport_id INT NOT NULL,
    runway_code VARCHAR(10) NOT NULL COMMENT 'e.g., 09L/27R',
    length_meters INT NOT NULL,
    surface_type ENUM('ASPHALT', 'CONCRETE', 'COMPOSITE') NOT NULL DEFAULT 'ASPHALT',
    status ENUM('ACTIVE', 'MAINTENANCE', 'CLOSED') NOT NULL DEFAULT 'ACTIVE',
    CONSTRAINT fk_runway_airport FOREIGN KEY (airport_id)
        REFERENCES airports(airport_id) ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT uq_airport_runway UNIQUE (airport_id, runway_code),
    CONSTRAINT chk_runway_length CHECK (length_meters >= 500)
) ENGINE=InnoDB;

CREATE INDEX idx_runways_airport ON runways(airport_id);

-- ----------------------------------------------------------------------------
-- 4. GATES TABLE
-- ----------------------------------------------------------------------------
CREATE TABLE gates (
    gate_id INT AUTO_INCREMENT PRIMARY KEY,
    airport_id INT NOT NULL,
    terminal VARCHAR(20) NOT NULL DEFAULT 'T1',
    gate_code VARCHAR(15) NOT NULL COMMENT 'e.g., A1, B12',
    gate_type ENUM('DOMESTIC', 'INTERNATIONAL', 'WIDEBODY') NOT NULL DEFAULT 'DOMESTIC',
    status ENUM('AVAILABLE', 'MAINTENANCE', 'CLOSED') NOT NULL DEFAULT 'AVAILABLE',
    CONSTRAINT fk_gate_airport FOREIGN KEY (airport_id)
        REFERENCES airports(airport_id) ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT uq_airport_gate UNIQUE (airport_id, gate_code)
) ENGINE=InnoDB;

CREATE INDEX idx_gates_airport ON gates(airport_id);

-- ----------------------------------------------------------------------------
-- 5. AIRCRAFT TABLE
-- ----------------------------------------------------------------------------
CREATE TABLE aircraft (
    aircraft_id INT AUTO_INCREMENT PRIMARY KEY,
    registration_no VARCHAR(20) NOT NULL UNIQUE COMMENT 'Tail number e.g. VT-ANB',
    model VARCHAR(80) NOT NULL,
    manufacturer VARCHAR(80) NOT NULL,
    total_seats INT NOT NULL,
    range_km INT NOT NULL,
    status ENUM('AVAILABLE', 'IN_FLIGHT', 'MAINTENANCE', 'RETIRED') NOT NULL DEFAULT 'AVAILABLE',
    current_airport_id INT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_aircraft_airport FOREIGN KEY (current_airport_id)
        REFERENCES airports(airport_id) ON DELETE SET NULL ON UPDATE CASCADE,
    CONSTRAINT chk_aircraft_seats CHECK (total_seats BETWEEN 10 AND 850),
    CONSTRAINT chk_aircraft_range CHECK (range_km > 100)
) ENGINE=InnoDB;

CREATE INDEX idx_aircraft_status ON aircraft(status);

-- ----------------------------------------------------------------------------
-- 6. FLIGHTS TABLE
-- ----------------------------------------------------------------------------
CREATE TABLE flights (
    flight_id INT AUTO_INCREMENT PRIMARY KEY,
    flight_number VARCHAR(15) NOT NULL UNIQUE,
    airline_name VARCHAR(100) NOT NULL,
    origin_airport_id INT NOT NULL,
    destination_airport_id INT NOT NULL,
    aircraft_id INT NOT NULL,
    departure_gate_id INT NOT NULL,
    arrival_gate_id INT NOT NULL,
    departure_runway_id INT NOT NULL,
    arrival_runway_id INT NOT NULL,
    scheduled_departure DATETIME NOT NULL,
    scheduled_arrival DATETIME NOT NULL,
    duration_minutes INT NOT NULL,
    priority_level TINYINT NOT NULL DEFAULT 3 COMMENT '1=Emergency/VIP, 2=International, 3=Domestic, 4=Cargo',
    base_fare DECIMAL(10, 2) NOT NULL DEFAULT 150.00,
    status ENUM('SCHEDULED', 'BOARDING', 'DEPARTED', 'IN_AIR', 'LANDED', 'DELAYED', 'CANCELLED') NOT NULL DEFAULT 'SCHEDULED',
    remarks VARCHAR(255) NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_flight_origin FOREIGN KEY (origin_airport_id)
        REFERENCES airports(airport_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT fk_flight_dest FOREIGN KEY (destination_airport_id)
        REFERENCES airports(airport_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT fk_flight_aircraft FOREIGN KEY (aircraft_id)
        REFERENCES aircraft(aircraft_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT fk_flight_dep_gate FOREIGN KEY (departure_gate_id)
        REFERENCES gates(gate_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT fk_flight_arr_gate FOREIGN KEY (arrival_gate_id)
        REFERENCES gates(gate_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT fk_flight_dep_runway FOREIGN KEY (departure_runway_id)
        REFERENCES runways(runway_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT fk_flight_arr_runway FOREIGN KEY (arrival_runway_id)
        REFERENCES runways(runway_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT chk_distinct_airports CHECK (origin_airport_id <> destination_airport_id),
    CONSTRAINT chk_flight_times CHECK (scheduled_arrival > scheduled_departure),
    CONSTRAINT chk_priority_range CHECK (priority_level BETWEEN 1 AND 4),
    CONSTRAINT chk_positive_fare CHECK (base_fare >= 0)
) ENGINE=InnoDB;

CREATE INDEX idx_flights_dep_time ON flights(scheduled_departure);
CREATE INDEX idx_flights_arr_time ON flights(scheduled_arrival);
CREATE INDEX idx_flights_status ON flights(status);
CREATE INDEX idx_flights_aircraft ON flights(aircraft_id);

-- ----------------------------------------------------------------------------
-- 7. PASSENGERS TABLE
-- ----------------------------------------------------------------------------
CREATE TABLE passengers (
    passenger_id INT AUTO_INCREMENT PRIMARY KEY,
    full_name VARCHAR(120) NOT NULL,
    email VARCHAR(120) NOT NULL UNIQUE,
    phone VARCHAR(25) NOT NULL,
    passport_number VARCHAR(30) NOT NULL UNIQUE,
    nationality VARCHAR(80) NOT NULL,
    date_of_birth DATE NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

CREATE INDEX idx_passengers_name ON passengers(full_name);

-- ----------------------------------------------------------------------------
-- 8. BOOKINGS TABLE
-- Enforces unique active seat lock per flight at DB engine level via generated column
-- ----------------------------------------------------------------------------
CREATE TABLE bookings (
    booking_id INT AUTO_INCREMENT PRIMARY KEY,
    booking_reference VARCHAR(12) NOT NULL UNIQUE,
    flight_id INT NOT NULL,
    passenger_id INT NOT NULL,
    seat_number VARCHAR(6) NOT NULL COMMENT 'e.g., 12A, 03F',
    seat_class ENUM('ECONOMY', 'BUSINESS', 'FIRST') NOT NULL DEFAULT 'ECONOMY',
    fare_paid DECIMAL(10, 2) NOT NULL,
    booking_status ENUM('CONFIRMED', 'CANCELLED') NOT NULL DEFAULT 'CONFIRMED',
    active_seat_lock TINYINT GENERATED ALWAYS AS (IF(booking_status = 'CONFIRMED', 1, NULL)) STORED,
    booked_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    cancelled_at DATETIME NULL,
    CONSTRAINT fk_booking_flight FOREIGN KEY (flight_id)
        REFERENCES flights(flight_id) ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_booking_passenger FOREIGN KEY (passenger_id)
        REFERENCES passengers(passenger_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT uq_active_flight_seat UNIQUE (flight_id, seat_number, active_seat_lock),
    CONSTRAINT chk_booking_fare CHECK (fare_paid >= 0)
) ENGINE=InnoDB;

CREATE INDEX idx_bookings_flight ON bookings(flight_id);
CREATE INDEX idx_bookings_passenger ON bookings(passenger_id);
CREATE INDEX idx_bookings_status ON bookings(booking_status);
