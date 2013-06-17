library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.numeric_std.all;

entity i2ctest is
  generic (slave_address: std_logic_vector(6 downto 0) := (others => '0'));
  port (i2c_scl: inout std_logic;
        i2c_sda: inout std_logic;
        CLOCK_50: in std_logic;
        KEY: in std_logic_vector(3 downto 0)
        );
end i2ctest;


architecture simulate of i2ctest is
  component i2c_master
    GENERIC(
      input_clk : INTEGER := 50_000_000; --input clock speed from user logic in Hz
      bus_clk   : INTEGER := 400_000);   --speed the i2c bus (scl) will run at in Hz
    PORT(
      clk       : IN     STD_LOGIC;                    --system clock
      reset_n   : IN     STD_LOGIC;                    --active low reset
      ena       : IN     STD_LOGIC;                    --latch in command
      addr      : IN     STD_LOGIC_VECTOR(6 DOWNTO 0); --address of target slave
      rw        : IN     STD_LOGIC;                    --'0' is write, '1' is read
      data_wr   : IN     STD_LOGIC_VECTOR(7 DOWNTO 0); --data to write to slave
      busy      : OUT    STD_LOGIC;                    --indicates transaction in progress
      data_rd   : OUT    STD_LOGIC_VECTOR(7 DOWNTO 0); --data read from slave
      ack_error : BUFFER STD_LOGIC;                    --flag if improper acknowledge from slave
      sda       : INOUT  STD_LOGIC;                    --serial data output of i2c bus
      scl       : INOUT  STD_LOGIC);                   --serial clock output of i2c bus
  end component;
  
  type machine is (reset,writing,finished);
  signal addr: std_logic_vector(6 downto 0) := slave_address;
  signal data_wr: std_logic_vector(7 downto 0) := (others => '0');
  signal reset_n: std_logic := '1';
  signal ena: std_logic := '0';
  signal rw: std_logic := '0';
  signal busy: std_logic;
  signal data_rd: std_logic_vector(7 downto 0);
  signal ack_error: std_logic;
  signal state: machine := reset;
  signal busy_prev: std_logic := '0';
  signal busy_count: integer := 0;
begin
  i2c_servoboard: i2c_master
    generic map (bus_clk => 400_000)
    port map(
      clk => CLOCK_50,
      reset_n => reset_n,
      ena => ena,
      addr => addr,
      rw => rw,
      data_wr => data_wr,
      busy => busy,
      data_rd => data_rd,
      ack_error => ack_error,
      sda => i2c_sda,
      scl => i2c_scl);
  
  process(CLOCK_50)
  begin
    -- TODO: Add error transitions.
    if rising_edge(CLOCK_50) then
      case state is
        -- Reset state. Keep i2c_master inactive too.
        when reset =>
          if KEY(0) = '1' then
            state <= writing;
          end if;
        -- Send the preamble (ff)
        when writing =>
          busy_prev <= busy;
          if busy_prev = '0' and busy = '1' then
            busy_count <= busy_count + 1;
          end if;
          case busy_count is
            when 0 =>
              -- Write preamble
              data_wr <= to_stdlogicvector(bit_vector'(x"FF"));
              ena <= '1';
            when 1 =>
              -- Set servo 0
              data_wr <= to_stdlogicvector(bit_vector'(x"00"));
            when 2 =>
              data_wr <= to_stdlogicvector(bit_vector'(x"1f"));
            when 3 =>
              data_wr <= to_stdlogicvector(bit_vector'(x"40"));
            when 4 =>
              ena <= '0';
              state <= finished;
            when others =>
              state <= finished;
              ena <= '0';
          end case;
        when finished =>
          -- do nothing.
      end case;
    end if;
  end process;
end simulate;
