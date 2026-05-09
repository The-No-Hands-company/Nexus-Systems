# Spam Classifier - Machine Learning Showcase

## Overview

The Spam Classifier is a complete machine learning pipeline demonstrating NexusLang's capabilities for data science applications. It showcases classification, feature extraction, training, evaluation, and prediction—essential components of any ML system. Despite its simplicity, it demonstrates realistic ML workflow and metrics.

## Domain: Machine Learning & Data Science

This application showcases NexusLang's suitability for:
- Machine learning pipelines
- Text classification
- Feature engineering
- Model training and evaluation
- Performance metrics computation
- Data science workflows
- Statistical analysis

## Features Demonstrated

### 1. Data Structures for ML
Complete data pipeline structures:

```nlpl
struct Email
  id as Integer
  subject as String
  body as String
  word_count as Integer
  is_spam as Boolean
end

struct FeatureVector
  email_id as Integer
  suspicious_word_count as Integer
  capitalization_ratio as Float
  special_char_count as Integer
  urgency_score as Float
  predicted_spam as Boolean
end
```

### 2. Feature Extraction
Transforming raw data into ML features:

```nlpl
function extract_features with email as Email and classifier as SpamClassifier 
  returns FeatureVector
  // Extract multiple feature types
end
```

Extracted features:
- **Suspicious Word Count**: Presence of common spam keywords
- **Capitalization Ratio**: Unusual use of capital letters
- **Special Character Count**: Frequency of punctuation
- **Urgency Score**: Temporal pressure indicators

### 3. Classification Model
Simple linear classifier combining features:

```nlpl
function predict_spam_score with features as FeatureVector returns Float
  // Weighted combination of features → probability score
end
```

Scoring approach:
- Weighted feature combination
- Normalized scores (0-1)
- Threshold-based decision boundary

### 4. Training Function
Adjusting classifier based on data:

```nlpl
function train_classifier with emails as List of Email 
  and classifier as SpamClassifier returns SpamClassifier
  // Analyze distribution, update parameters
end
```

### 5. Evaluation Metrics
Comprehensive classification metrics:

```nlpl
struct ClassificationMetrics
  true_positives as Integer
  false_positives as Integer
  false_negatives as Integer
  true_negatives as Integer
  accuracy as Float      // (TP+TN) / Total
  precision as Float     // TP / (TP+FP)
  recall as Float        // TP / (TP+FN)
  f1_score as Float      // Harmonic mean of precision/recall
end
```

Standard ML metrics:
- **Accuracy**: Overall correctness
- **Precision**: Reliability of positive predictions
- **Recall**: Coverage of actual positives
- **F1-Score**: Harmonic mean balancing precision/recall

### 6. Contracts Throughout
Validation at every step:

```nlpl
require email is not null with "Email cannot be null"
require predictions.length equals actuals.length with "Count mismatch"
require classifier.trained equals true with "Must train before predict"
```

## Execution Example

Running the spam classifier:

```bash
python src/main.py examples/ml/spam_classifier.nlpl
```

Expected output:
```
=== Spam Classifier Demo ===

Trained on 8 emails
Threshold: 0.4

=== Classification Report ===

Confusion Matrix:
  True Positives: 4
  False Positives: 0
  True Negatives: 4
  False Negatives: 0

Performance Metrics:
  Accuracy: 100%
  Precision: 100%
  Recall: 100%
  F1-Score: 1.0

Sample Predictions:
  Email 1: Predicted=SPAM, Actual=SPAM
  Email 2: Predicted=SPAM, Actual=SPAM
  Email 3: Predicted=SPAM, Actual=SPAM
  Email 4: Predicted=HAM, Actual=HAM
  Email 5: Predicted=HAM, Actual=HAM
  Email 6: Predicted=HAM, Actual=HAM
  Email 7: Predicted=SPAM, Actual=SPAM
  Email 8: Predicted=HAM, Actual=HAM

Classification completed successfully
```

## Machine Learning Concepts

### Classification Fundamentals

**Binary Classification**: Predict one of two classes
- Spam vs. Not Spam (Ham)
- Disease vs. Healthy
- Fraud vs. Legitimate

**Feature Engineering**: Converting raw data to ML features
```nlpl
// Raw: "CONGRATULATIONS!!! YOU WON!!!"
// Features: 
//   - suspicious_words: 1 ("congratulations")
//   - capitalization_ratio: high (all caps)
//   - special_chars: 3 (!!!)
//   - urgency_score: high (winner + punctuation)
```

**Training vs. Inference**
- **Training**: Learn patterns from labeled examples
- **Inference**: Predict on new, unlabeled data

### Model Evaluation

**Confusion Matrix** (2x2 table):
```
              Predicted Positive    Predicted Negative
Actual Pos    True Positive (TP)    False Negative (FN)
Actual Neg    False Positive (FP)   True Negative (TN)
```

**Metrics Interpretation**:
- **Accuracy**: 95% means 95 out of 100 predictions correct
- **Precision**: 90% means when we predict spam, we're right 90% of the time
- **Recall**: 85% means we catch 85% of actual spam
- **F1**: Balanced metric useful for imbalanced datasets

### Overfitting vs. Underfitting

**Overfitting**: Model memorizes training data
- High training accuracy, low test accuracy
- Solution: More data, regularization

**Underfitting**: Model too simple for task
- Low accuracy on both training and test
- Solution: More complex model, more features

## Real-World Extensions

### Advanced Feature Engineering
```nlpl
// Semantic features
function extract_semantic_features with email returns Float
  // Sentiment analysis, topic modeling
end

// N-gram features
function extract_ngrams with text returns List of String
  // Common word pairs, word frequency
end

// Metadata features
function extract_metadata_features with email returns FeatureVector
  // Sender reputation, send time patterns
end
```

### Model Improvements
```nlpl
// Multi-class classification
function classify_email_category returns String
  // Spam, Phishing, Legitimate, Newsletter
end

// Ensemble methods
function ensemble_prediction with predictions as List of Boolean returns Boolean
  // Combine multiple models via voting
end

// Online learning
async function update_model_stream with email_stream as Channel
  // Continuous learning as new emails arrive
end
```

### Integration with Web Services
```nlpl
// REST API endpoint
function handle_classify_request with email_json as String returns String
  // Parse JSON, classify, return result
end

// Database persistence
function save_trained_model with model as SpamClassifier and filename as String
  // Serialize model for future use
end
```

## Performance Characteristics

### Computational Complexity
| Operation | Complexity | Time |
|-----------|-----------|------|
| Feature extraction | O(e) | ~microseconds per email |
| Prediction | O(f) | ~microseconds (f=features) |
| Training | O(n×e) | ~milliseconds (n=samples) |
| Evaluation | O(n) | ~microseconds |

### Scalability
- **Small**: 1K emails, single machine
- **Medium**: 100K emails, parallel processing
- **Large**: Millions of emails, distributed system

## Educational Value

This example teaches:

### Machine Learning Concepts
1. **Supervised Learning**: Learning from labeled data
2. **Feature Engineering**: Converting raw data to useful features
3. **Model Training**: Adjusting parameters from data
4. **Classification**: Predicting discrete categories
5. **Evaluation Metrics**: Assessing model performance

### NexusLang Programming
1. Complex data structures for ML pipelines
2. Function composition for data flow
3. Contracts for data validation
4. Collection operations for batch processing
5. Mathematical computations

### Software Engineering
1. Clean separation of concerns
2. Testable functions
3. Metrics-driven development
4. Production-ready code patterns

## Testing Strategy

Contract enforcement catches data errors:

```nlpl
// This would fail:
set invalid_email to create_email with -1 and "" and "text" and 0 and false
// Error: "Email ID must be non-negative"
// Error: "Subject cannot be empty"
// Error: "Word count must be positive"

// This would fail:
set metrics to calculate_metrics with predictions and different_actuals
// Error: "Prediction and actual counts must match"
```

## Real-World Applications

### Email Filtering
- Reduce user inbox clutter
- Prevent phishing attacks
- Organize newsletters

### Anomaly Detection
- Fraudulent transactions
- Network intrusions
- System failures

### Sentiment Analysis
- Customer feedback
- Brand monitoring
- Social media analysis

### Medical Diagnosis
- Disease classification
- Risk stratification
- Treatment recommendation

## Limitations of Current Implementation

1. **Simple Model**: Linear combination, no interactions
2. **Limited Features**: Text-only, no metadata
3. **Manual Feature Engineering**: Not learned from data
4. **No Hyperparameter Tuning**: Fixed threshold
5. **No Cross-Validation**: Single train-test split
6. **Memory-Based**: All data in memory

## Future Enhancements

### Advanced Algorithms
- **Neural Networks**: Deep learning for better patterns
- **Gradient Boosting**: Ensemble of weak learners
- **Support Vector Machines**: Non-linear decision boundaries
- **Probabilistic Models**: Bayesian classification

### Scalability
- **Streaming Learning**: Process data as it arrives
- **Distributed Training**: Parallelize across cluster
- **Feature Caching**: Pre-compute expensive features
- **Model Compression**: Reduce size for deployment

### Production Considerations
- **Model Versioning**: Track model history
- **Performance Monitoring**: Track metrics over time
- **A/B Testing**: Compare models in production
- **Fairness Analysis**: Check for bias

## Related Examples

See also:
- [Graph Analyzer](../algorithms/graph_analyzer.nlpl) - Algorithmic patterns
- [Portfolio Analyzer](../financial/portfolio_analyzer.nlpl) - Data processing
- [Log Analyzer](../systems/log_analyzer.nlpl) - Text processing and aggregation

## Conclusion

The Spam Classifier demonstrates that NexusLang is capable of implementing complete machine learning pipelines, from data preparation through evaluation. While simplified compared to production ML frameworks, it shows that NexusLang's:

- **Type system** enables safe feature vectors
- **Contracts** validate data at each pipeline stage
- **Collections** handle datasets efficiently
- **Readability** makes ML logic clear
- **Performance** is suitable for real applications

This makes NexusLang viable for machine learning applications ranging from educational demonstrations to production systems—proving it's truly a general-purpose language suitable for any programming domain.
