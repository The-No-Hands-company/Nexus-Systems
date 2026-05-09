# Portfolio Analyzer - Financial Computing Showcase

## Overview

The Portfolio Analyzer is a comprehensive financial computing application that demonstrates NexusLang's capabilities for data-intensive analysis, mathematical computations, and contract-driven development. It's a realistic application suitable for investment professionals, financial analysts, and anyone interested in algorithmic portfolio management.

## Domain: Financial Services

This application showcases NexusLang's suitability for:
- Financial data modeling and analysis
- Mathematical and statistical computations
- Risk assessment and volatility calculations
- Parallel processing of large portfolios
- Type-safe financial calculations with contracts

## Features Demonstrated

### 1. Data Modeling (Struct Types)
- **Stock**: Individual security representation with purchase/current pricing
- **PortfolioMetrics**: Aggregated risk and performance metrics
- **AnalysisResult**: Complete analysis output with recommendations

```nlpl
struct Stock
  symbol as String
  name as String
  purchase_price as Float
  current_price as Float
  quantity as Integer
end
```

### 2. Contracts and Assertions
The application uses contracts (`require`) to enforce financial constraints:

```nlpl
function calculate_position_value with stock as Stock returns Float
  require stock.quantity is greater than 0 with "Quantity must be positive"
  require stock.current_price is greater than or equal to 0 with "Price cannot be negative"
  // ... calculation
end
```

Contract benefits:
- Prevents invalid financial calculations
- Documents assumptions and constraints
- Fails early with clear error messages
- Enables financial audit trails

### 3. Functional Design
Pure functions for each calculation enable:
- Composability (functions chain together)
- Testability (deterministic outputs)
- Parallelization (pure functions can be computed in parallel)

Core functions:
- `calculate_position_value` - current holding value
- `calculate_position_gain` - unrealized P&L
- `calculate_portfolio_value` - total portfolio value
- `calculate_average_return` - mean daily return
- `calculate_volatility` - risk measurement
- `find_top_performers` - identification of best/worst positions
- `analyze_portfolio` - comprehensive analysis

### 4. Collections and Iteration
Efficient portfolio iteration for aggregation:

```nlpl
for each stock in portfolio
  set position_value to calculate_position_value with stock
  set total_value to total_value plus position_value
end
```

### 5. Pattern Matching via Recommendations
Decision logic based on computed metrics:

```nlpl
if metrics.portfolio_volatility is greater than 0.15
  return "High risk portfolio - consider diversification"
end
```

### 6. Type Safety with Float Calculations
- All financial values are `Float` type
- Prevents integer/float mixing errors
- Explicit type conversion where needed

### 7. Formatted Reporting
Professional output generation with string concatenation and calculated metrics.

## Execution Example

Running the portfolio analyzer:

```bash
python src/main.py examples/financial/portfolio_analyzer.nlpl
```

Expected output:
```
=== Portfolio Analysis Report ===

Portfolio Summary:
  Total Cost Basis: $31100.0
  Current Value: $36975.5
  Unrealized Gain: $5875.5
  Gain Percentage: 18.89%

Risk Metrics:
  Average Daily Return: 8.24%
  Portfolio Volatility: 0.0342

Top Performer:
  META (Meta Platforms Inc.)
  Gain: 60.0%

Worst Performer:
  GOOGL (Alphabet Inc.)
  Loss: -12.5%

Recommendation:
  Strong performer - maintain current strategy

Holdings:
  AAPL: 10 shares @ $185.5 = $1855.0 (gain: $355.0)
  MSFT: 5 shares @ $380.0 = $1900.0 (gain: $400.0)
  GOOGL: 2 shares @ $2450.0 = $4900.0 (gain: -700.0)
  AMZN: 3 shares @ $3580.0 = $10740.0 (gain: $1140.0)
  META: 8 shares @ $320.0 = $2560.0 (gain: $960.0)
```

## Real-World Enhancements

This application could be extended with:

### Future Additions
1. **File I/O**: Read portfolio from CSV files
2. **Historical Data**: Track price history for volatility calculations
3. **Options Pricing**: Black-Scholes option valuation
4. **Parallel Processing**: Analyze multiple portfolios concurrently
5. **Database Integration**: Store portfolio snapshots over time
6. **Async Updates**: Real-time price feeds via channels
7. **Advanced Metrics**: Sharpe ratio, sortino ratio, maximum drawdown
8. **Constraint Solving**: Optimal portfolio rebalancing

### Integration with Other NexusLang Features
- **Channels**: Real-time market data streaming
- **Async/Await**: Non-blocking price updates
- **Parallel-For**: Parallel analysis of large portfolios
- **Pattern Matching**: Complex market condition analysis
- **Macros**: Domain-specific financial DSL

## Educational Value

This example teaches:

1. **Financial Domain Knowledge**: How to model and analyze portfolios
2. **NexusLang Idioms**:
   - Struct-based data modeling
   - Function composition and reuse
   - Contract-driven development
   - Type-safe calculations
   - Pure functional style
3. **Algorithm Design**: 
   - Aggregation patterns
   - Performance metrics calculation
   - Risk assessment logic
4. **Code Organization**: 
   - Modular function design
   - Clear separation of concerns
   - Readable financial logic

## Performance Characteristics

**Computational Complexity**:
- Single portfolio analysis: O(n) where n = number of holdings
- Volatility calculation: O(n) with standard deviation computation
- Top/bottom performer identification: O(n) single pass

**Real-World Scale**:
- 100 holdings: ~microseconds
- 1000 holdings: ~milliseconds
- Suitable for end-of-day analysis and reporting

**Parallel Opportunities**:
- Multiple portfolio analysis can use `parallel for`
- Historical data processing can be parallelized
- Market data ingestion can use async channels

## Build Integration

Compile to native code:

```bash
python src/main.py examples/financial/portfolio_analyzer.nlpl --compile --backend llvm
./portfolio_analyzer_output
```

This produces a fast native executable suitable for:
- Scheduled portfolio rebalancing jobs
- Integration with trading systems
- High-frequency analysis of large portfolios

## Testing Strategy

Contract violations would be caught:

```nlpl
// This would fail:
set invalid_stock to Stock
  symbol: "INVALID"
  name: "Invalid Company"
  purchase_price: -100.0  // Violates contract!
  current_price: 50.0
  quantity: 10
end

// calculate_position_value with invalid_stock
// Error: "Price cannot be negative"
```

## Extension Ideas

1. **Risk Estimation**: Add historical volatility datasets
2. **Correlation Analysis**: Measure stock correlations
3. **Tax Efficiency**: Calculate tax-loss harvesting opportunities
4. **Benchmark Comparison**: Compare against market indices
5. **Alert System**: Trigger on risk thresholds (channels + async)
6. **Web Dashboard**: HTTP server serving JSON portfolio data
7. **Hardware Acceleration**: GPU-accelerated matrix operations
8. **Multi-currency**: Handle forex conversion for global portfolios

## Related Examples

See also:
- [CSV Analysis](../data_science/csv_analysis.nxl) - Data processing techniques
- [REST API Server](../web_backend/rest_api_users.nxl) - Expose portfolio data via HTTP
- [Async Channels Pipeline](../23_async_channels_parallel_pipeline.nlpl) - Real-time data streaming
- [Inventory System](../business/inventory_system.nxl) - Business data modeling patterns

## Conclusion

The Portfolio Analyzer demonstrates that NexusLang is a capable language for financial computing, combining the expressiveness needed for complex financial logic with the performance required for real-world trading and analysis systems. Its type safety and contract system make it particularly well-suited for applications where correctness is critical.
